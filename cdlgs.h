// This file is part of the cwiclui project
//
// Copyright (c) 2019 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the ISC License.

#pragma once
#include "window.h"

namespace cwiclui {

//{{{ IMessageBox ------------------------------------------------------

class IMessageBox : public Interface {
    DECLARE_INTERFACE (Interface, MessageBox, (ask,"qqs")(answer,"q"))
public:
    enum class Answer : uint16_t { Cancel, Ok, Ignore, Yes = Ok, Retry = Ok, No = Ignore };
    enum class Type : uint16_t { Ok, OkCancel, YesNo, YesNoCancel, RetryCancelIgnore };
public:
    explicit	IMessageBox (mrid_t caller) : Interface (caller) {}
    void	ask (const string& prompt, Type type = Type()) const
		    { send (m_ask(), type, uint16_t(0), prompt); }
    template <typename O>
    inline static constexpr bool dispatch (O* o, const Msg& msg) {
	if (msg.method() != m_ask())
	    return false;
	auto is = msg.read();
	auto type = is.read<Type>();
	auto flags = is.read<uint16_t>();
	auto prompt = is.read<string_view>();
	o->MessageBox_ask (prompt, type, flags);
	return true;
    }
public:
    class Reply : public Interface::Reply {
    public:
	explicit constexpr Reply (Msg::Link l) : Interface::Reply (l) {}
	void reply (Answer answer) const { send (m_answer(), answer); }
	template <typename O>
	inline static constexpr bool dispatch (O* o, const Msg& msg) {
	    if (msg.method() != m_answer())
		return false;
	    o->MessageBox_reply (msg.read().read<Answer>());
	    return true;
	}
    };
};

//}}}-------------------------------------------------------------------

class MessageBox : public Window {
    using Type = IMessageBox::Type;
    using Answer = IMessageBox::Answer;
    IMPLEMENT_INTERFACES_I (Window, (IMessageBox),)
public:
    explicit		MessageBox (Msg::Link l);
    inline void		MessageBox_ask (const string_view& prompt, Type type, uint16_t flags);
    void		on_key (key_t key) override;
private:
    void		done (Answer answer);
private:
    string		_prompt;
    Type		_type;
    enum : widgetid_t {
	wid_Frame = wid_First,
	wid_Message,
	wid_Cancel,
	wid_Ignore,
	wid_OK
    };
    static constexpr const Layout c_layout[] = {
	WL_(GroupFrame,	wid_Frame),
	WL___(Label,	wid_Message),
	WL___(HBox,	HAlign::Center),
	WL_____(Button,	wid_OK),
	WL_____(Button,	wid_Cancel),
	WL_____(Button,	wid_Ignore)
    };
};

} // namespace cwiclui
