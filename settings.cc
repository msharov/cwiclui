// This file is part of the p1 project
//
// Copyright (c) 2017 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the LGPLv3 license.

#include "settings.h"
#include <sys/stat.h>

//{{{ SettingsKey ------------------------------------------------------
namespace cwiclui {

constexpr const char SettingsKey::c_Bool_names[]; // static

SettingsKey::SettingsKey (const char* path, const char* filename, time_t modified)
:_entries()
,_modified (modified)
{
    assert (!strchr(path,']') && !strchr(path,'\n') && "invalid character in key path");
    _entries.assign (path, strlen(path)+1);
    _entries.append (filename, strlen(filename)+1);
}

SettingsKey::size_type SettingsKey::size (void) const
{
    size_type n = 0;
    foreach (i, *this)
	++n;
    return n;
}

void SettingsKey::set_filename (const string_view& filename)
{
    auto ei = ebegin();
    auto kfn = *++ei;
    auto kfnend = *++ei;
    _entries.replace (kfn, kfnend-kfn-1, filename.begin(), filename.size());
}

void SettingsKey::subtract (const SettingsKey& k)
{
    for (auto& ke : k) {
	auto me = get_entry (ke.name);
	if (me && !strcmp (me->value, ke.value))
	    erase (me);
    }
}

void SettingsKey::merge (const SettingsKey& k)
{
    auto kfn = k.filename();
    if (kfn[0])
	set_filename (kfn);
    if (k.modified() > modified())
	set_modified (k.modified());
    for (auto& e : k)
	set_entry (e.name, e.value, e.description);
}

bool SettingsKey::is_valid (void) const
{
    auto ei = ebegin();
    if (ei.remaining() < 2)
	return false;
    auto nstr = 0u;
    for (; ei; ++ei)
	++nstr;
    // Each key must have keypath, filename, and entries 3 strings each
    return nstr >= 2 && ((nstr-2) % 3) == 0;
}

auto SettingsKey::get_entry (const char* name) const -> const_iterator
    { return find (begin(), end(), name); }
long SettingsKey::get_entry_int (const char* name, long dv) const
    { auto e = get_entry (name); return e ? e->int_value() : dv; }
double SettingsKey::get_entry_float (const char* name, double dv) const
    { auto e = get_entry (name); return e ? e->float_value() : dv; }
bool SettingsKey::get_entry_bool (const char* name, bool dv) const
    { auto e = get_entry (name); return e ? e->bool_value() : dv; }
unsigned SettingsKey::get_entry_enum (const char* name, const char* evn, size_t evnsz, unsigned dev) const
    { auto e = get_entry (name); return e ? e->enum_value (evn, evnsz, dev) : dev; }

void SettingsKey::get_entry_str (const char* name, string& s) const
{
    if (auto e = get_entry (name); e)
	s = e->str_value();
    else
	s.clear();
}

void SettingsKey::get_entry_str (const char* name, string& s, const string_view& dv) const
{
    if (auto e = get_entry (name); e)
	s = e->str_value();
    else
	s = dv;
}

void SettingsKey::get_entry_str (const char* name, string_view& s) const
{
    if (auto e = get_entry (name); e)
	s = e->str_value();
    else
	s.clear();
}

void SettingsKey::get_entry_str (const char* name, string_view& s, const string_view& dv) const
{
    if (auto e = get_entry (name); e)
	s = e->str_value();
    else
	s = dv;
}

void SettingsKey::get_entry_array (const char* name, vector<string>& v) const
{
    string_view s;
    get_entry_str (name, s);
    v.clear();
    auto ai = s.find ('[');
    if (!ai)
	return;
    auto alast = s.find (']', ai);
    if (!alast)
	return;
    while (++ai < alast) {
	auto sstart = ai;
	while (*sstart == ' ')
	    ++sstart;
	auto send = sstart;
	while (*send != ',' && *send != ']')
	    ++send;
	ai = send+1;
	while (send > sstart && send[-1] == ' ')
	    --send;
	v.emplace_back (sstart, send);
    }
}

void SettingsKey::get_entry_array (const char* name, vector<string_view>& v) const
{
    string_view s;
    get_entry_str (name, s);
    v.clear();
    auto ai = s.find ('[');
    if (!ai)
	return;
    auto alast = s.find (']', ai);
    if (!alast)
	return;
    while (++ai < alast) {
	auto sstart = ai;
	while (*sstart == ' ')
	    ++sstart;
	auto send = sstart;
	while (*send != ',' && *send != ']')
	    ++send;
	ai = send+1;
	while (send > sstart && send[-1] == ' ')
	    --send;
	v.emplace_back (sstart, send);
    }
}

void SettingsKey::set_entry (const char* name, const char* value, const char* desc)
{
    assert (name[0] != '[' && !strchr(name,'=') && !strchr(name,'\n') && "invalid character in entry name");
    assert (!value || (!strchr(value,'\n') && "invalid character in entry value"));

    // Entries are sorted; find the insertion point
    int cmpr = 1;
    auto ip = begin(), ipend = end();
    while (ip < ipend && 0 < (cmpr = strcmp (name, ip->name)))
	++ip;

    if (!desc)
	desc = "";

    // value is nullptr means delete the entry
    if (!value) {
	if (!cmpr)	// !cmpr means entry found
	    erase (ip);
	return;
    }
    const auto namelen = strlen(name)+1, valuelen = strlen(value)+1, desclen = strlen(desc)+1;
    if (cmpr) {
	// New entry, insert name, value and desc
	auto iname = _entries.insert (ip->name, name, namelen);
	auto ivalue = _entries.insert (iname + namelen, value, valuelen);
	_entries.insert (ivalue + valuelen, desc, desclen);
    } else {
	// Old entry exists. Replace value.
	auto ivalue = _entries.replace (ip->value, ip->description - ip->value, value, valuelen);
	if (desclen <= 1)	// keep old description if new one is empty
	    _entries.replace (ivalue + valuelen, ip->next - ip->description, desc, desclen);
    }
}

void SettingsKey::set_entry_int (const char* name, long value, const char* desc, long dv)
{
    char nbuf [32];
    snprintf (ARRAY_BLOCK(nbuf), "%ld", value);
    set_entry (name, value == dv ? nullptr : nbuf, desc);
}

void SettingsKey::set_entry_float (const char* name, double value, const char* desc, double dv)
{
    char nbuf [32];
    snprintf (ARRAY_BLOCK(nbuf), "%g", value);
    set_entry (name, value == dv ? nullptr : nbuf, desc);
}

void SettingsKey::set_entry_str (const char* name, const string_view& value)
{
    set_entry (name, value.empty() ? nullptr : value.c_str());
}

void SettingsKey::set_entry_str (const char* name, const string_view& value, const string_view& dv)
{
    set_entry (name, value == dv ? nullptr : value.c_str());
}

void SettingsKey::set_entry_enum (const char* name, unsigned ev, const char* evn, size_t evnsz, unsigned evd)
{
    set_entry (name, ev == evd ? nullptr : zstr::at (ev, evn, evnsz));
}

void SettingsKey::set_entry_array (const char* name, const vector<string>& v)
{
    string s;
    char sep = '[';
    for (auto& i : v) {
	s.appendf ("%c%s", sep, i.c_str());
	sep = ',';
    }
    s += ']';
    set_entry_str (name, s);
}

void SettingsKey::set_entry_array (const char* name, const vector<string_view>& v)
{
    string s;
    char sep = '[';
    for (auto& i : v) {
	s += sep;
	s += i;
	sep = ',';
    }
    s += ']';
    set_entry_str (name, s);
}

void SettingsKey::read (istream& is)
{
    _entries.clear();
    string_view str;
    uint32_t ne = 2u;
    for (auto i = 0u; i < ne; ++i) {
	is >> str;
	_entries.append (str.c_str(), str.size()+1);
	if (i == 1) {
	    _modified = is.read<uint32_t>();
	    ne += 3*is.read<uint32_t>();
	}
    }
}

void SettingsKey::write (sstream& os) const
{
    string_view str;
    for (auto ei = ebegin(); ei;) {
	auto s = *ei, ns = *++ei;
	str.link (s, ns-s-1);
	os << str;
    }
    os << uint32_t(modified()) << uint32_t(0);
}

void SettingsKey::write (ostream& os) const
{
    unsigned nw = 0;
    string_view str;
    for (auto ei = ebegin(); ei; ++nw) {
	auto s = *ei, ns = *++ei;
	str.link (s, ns-s-1);
	os << str;
	if (nw == 1)	// Path, filename, then modified and size
	    os << uint32_t(modified()) << uint32_t(size());
    }
}

//}}}-------------------------------------------------------------------
//{{{ Settings

bool Settings::is_valid (void) const
{
    for (size_type i = 0; i < size(); ++i)	// check each key valid and in order
	if (!at(i).is_valid() || (i > 0 && at(i) < at(i-1)))
	    return false;
    return true;
}

void Settings::merge_key (const SettingsKey& k)
{
    auto okey = _keys.lower_bound (k);
    if (okey < _keys.end() && *okey == k)
	okey->merge (k);
    else
	_keys.insert (okey, k);
}

void Settings::merge_key (SettingsKey&& k)
{
    auto okey = _keys.lower_bound (k);
    if (okey < _keys.end() && *okey == k)
	okey->merge (move(k));
    else
	_keys.insert (okey, move(k));
}

void Settings::set_key (const SettingsKey& k)
{
    auto okey = _keys.lower_bound (k);
    if (okey < _keys.end() && *okey == k)
	*okey = k;
    else
	_keys.insert (okey, k);
}

void Settings::set_key (SettingsKey&& k)
{
    auto okey = _keys.lower_bound (k);
    if (okey < _keys.end() && *okey == k)
	*okey = move(k);
    else
	_keys.insert (okey, move(k));
}

void Settings::match (Settings& result, const string_view& path, unsigned depth) const
{
    for (auto k = _keys.lower_bound (path.c_str()), kend = _keys.end(); k < kend; ++k) {
	auto kpath = k->path();

	// match all keys starting with path
	if (kpath[0] && !k->is_on_path (path))
	    break;

	// match up to depth; path/x/y has depth of 2
	auto kdepth = 0u;
	for (auto kpp = kpath+path.size(); *kpp; ++kpp)
	    if (*kpp == '/')
		++kdepth;
	if (kdepth > depth)
	    continue;

	result.merge_key (*k);
    }
}

void Settings::extract_match_from (Settings&& src, const string_view& path)
{
    foreach (i, src) {
	if (0 != strncmp (path.c_str(), i->path(), path.size()))
	    continue;
	merge_key (move(*i));
	--(i = src.erase(i));
    }
}

auto Settings::create_key (const char* path) -> reference
{
    auto okey = _keys.lower_bound (path);
    if (okey < _keys.end() && *okey == path)
	return *okey;
    auto filename = "";
    time_t modified = 0;
    if (!empty()) {
	filename = at(0).filename();
	modified = at(0).modified();
    }
    return *_keys.emplace (path, filename, modified);
}

// Deletes the given key and all its subkeys
auto Settings::delete_key (const string_view& path) -> size_type
{
    auto oldsize = _keys.size();
    remove_if (_keys, [&](auto& k)
	{ return 0 == strncmp (path.c_str(), k.path(), path.size()); });
    return oldsize - _keys.size();	// returns number of keys deleted
}

void Settings::create_all_paths (void)
{
    auto lastpath = "";
    foreach (k, _keys) {
	auto curpath = k->path();
	auto slashpos = strrchr (curpath, '/');
	size_t curpathdirsz = slashpos ? slashpos - curpath : 0;
	//
	// Keys are sorted, so the previous key's path must contain the dirname
	// of the current one. If it does not, the parent key must be created.
	//
	if (!curpathdirsz || 0 == strncmp (lastpath, curpath, curpathdirsz)) {
	    lastpath = curpath;
	    continue;
	}
	// Create new parent key, copying values from child
	char parentpath [curpathdirsz+1];
	copy_n (curpath, curpathdirsz, parentpath)[0] = 0;
	--(k = _keys.emplace_hint (k, &parentpath[0], k->filename(), k->modified()));
    }
}

//}}}-------------------------------------------------------------------
//{{{ Settings INI file parser

namespace {
inline static constexpr bool is_space (char c)
    { return c == ' ' || c == '\t'; }

static void trim_spaces (char*& fp, char*& lp)
{
    auto f = fp, l = lp;
    while (f < l && is_space(*f)) ++f;
    while (l > f && is_space(l[-1])) --l;
    fp = f; lp = l;
}
}

void Settings::read_ini_file (const char* filename, const char* sfn)
{
    struct stat st;
    if (0 > ::stat (filename, &st))
	return;
    read_ini (string::create_from_file (filename), sfn, st.st_mtime);
}

void Settings::read_ini (string fbuf, const char* sfn, time_t mtime)
{
    // First empty key added to keep the filename
    if (empty())
	create_key ("");
    at(0).set_filename (sfn);
    if (mtime > at(0).modified())
	at(0).set_modified (mtime);

    SettingsKey* key = nullptr;
    string descbuf;	// to store multiline descriptions
    auto fend = fbuf.end(), line = fbuf.begin(), nline = line;
    do {
	// Parse one line at a time
	auto lend = strchr (line, '\n');
	if (!lend)
	    nline = lend = fend;
	else
	    nline = lend+1;
	trim_spaces (line, lend);

	// [key]
	if (*line == '[') {
	    if (lend[-1] == ']') {
		// Extract key name from brackets
		++line, --lend;
		trim_spaces (line, lend);
		*lend = 0;	// terminate it,
		// and look it up or create it in the settings array
		key = &create_key (line);
		key->set_filename (sfn);
		if (mtime > key->modified())
		    key->set_modified (mtime);
	    }
	} else if (key) {	// entries are only valid in key scope
	    if (*line == '#') {	// description, possibly multiline
		if (is_space(*++line))	// skip one space, if present
		    ++line;
		if (!descbuf.empty())
		    descbuf.push_back ('\n');
		descbuf.append (line, lend-line);
	    } else if (auto eq = static_cast<char*>(memchr(line, '=', lend-line)); eq) {	// entry: name = value
		auto name = line, ename = eq, value = eq+1, evalue = lend;
		trim_spaces (name, ename);
		trim_spaces (value, evalue);
		*ename = *evalue = 0;
		key->set_entry (name, value, descbuf.empty() ? "" : descbuf.c_str());
		descbuf.clear();
	    }
	}
    } while ((line = nline) < fend);
    assert (is_valid());
}

string Settings::write_ini (void) const
{
    string o;
    for (auto& k : _keys) {
	if (!k.path()[0] || !k.size())
	    continue;
	o.appendf ("%s[%s]\n", &"\n"[o.empty()], k.path());
	for (auto& e : k) {
	    auto d = e.description;
	    while (*d) {
		auto ndl = strchr(d,'\n');
		if (!ndl)
		    ndl = d+strlen(d);
		o.append ("# ", strlen("# "));
		o.append (d, ndl-d);
		d = ndl+(*ndl == '\n');
		o += '\n';
	    }
	    o.appendf ("%s=%s\n", e.name, e.value);
	}
    }
    return o;
}

} // namespace cwiclui
//}}}-------------------------------------------------------------------
