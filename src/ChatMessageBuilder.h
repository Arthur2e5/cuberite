
// ChatMessageBuilder.h

// Declares the cChatMessageBuilder class used to build a chat message

#include "Defines.h"
#include "json/json.h"
#include "Logger.h"
#include "ChatColor.h"

// tolua_begin
class cChatMessageBuilder
{
public:
	// tolua_end

	/** This class is used to store the text, the click event, hover event and insertion text. */
	class cChatMessagePart
	{
	public:
		cChatMessagePart(const AString & a_Text):
			m_Text(a_Text)
		{
		}

		~cChatMessagePart() {};

		AString m_Text;
		AString m_InsertionText;
		AString m_HoverText;
		AString m_ClickText;
		AString m_HoverAction;
		AString m_ClickAction;
	} ;

	std::vector<cChatMessagePart *> m_Parts;

	// tolua_begin

	cChatMessageBuilder(void) :
		m_MessageType(mtCustom)
	{
	}

	~cChatMessageBuilder();

	void Clear(void);

	/** Adds a new cChatMessagePart and sets the text. Returns cChatMessageBuilder  */
	void AppendPart(const AString & a_Text);

	/** Sets a action on the last part in m_Parts, that will be run, if the text has been clicked
	Actions:
	- open_url
	- run_command
	- suggest_command
	*/
	void SetClickEvent(const AString & a_Action, const AString & a_Text);

	/** Sets a action on the last part in m_Parts that will be run, if the mouse is over the text
	Actions:
	- show_text
	- show_achievement
	- show_item
	*/
	void SetHoverEvent(const AString & a_Action, const AString & a_Text);

	/** Inserts the specified text into the chat on Shift Click (>= 1.8) */
	void SetInsertionText(const AString & a_Text);

	AString CreateJsonString(bool a_ShouldUseChatPrefixes = true) const;

	/** Returns the message type set previously by SetMessageType(). */
	eMessageType GetMessageType(void) const { return m_MessageType; }

	/** Returns additional data pertaining to message type, for example, the name of a mtPrivateMsg sender */
	AString GetAdditionalMessageTypeData(void) const { return m_AdditionalMessageTypeData; }

	/** Sets the message type, which is indicated by prefixes added to the message when serializing
	Takes optional AdditionalMessageTypeData to set m_AdditionalMessageTypeData. See said variable for more documentation. */
	void SetMessageType(eMessageType a_MessageType, const AString & a_AdditionalMessageTypeData = "");

	// tolua_end
protected:
	/** The message type, as indicated by prefixes. */
	eMessageType m_MessageType;

	/** Additional data pertaining to message type, for example, the name of a mtPrivateMsg sender */
	AString m_AdditionalMessageTypeData;
} ; // tolua_export
