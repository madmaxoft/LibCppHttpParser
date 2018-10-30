#pragma once

#include <string>
#include <map>





namespace Http {





/** Parses strings in the "name=value;name2=value2" format into a stringmap.
The data is pushed into the parser and it keeps internal parsing state, until finish() is called. */
class NameValueParser:
	public std::map<std::string, std::string>
{
public:

	/** Creates an empty parser */
	NameValueParser(bool aAllowsKeyOnly = true);

	/** Creates an empty parser, then parses the data given. Doesn't call Finish(), so more data can be parsed later */
	NameValueParser(const char * aData, size_t aSize, bool aAllowsKeyOnly = true);

	/** Parses the data given */
	void parse(const char * aData, size_t aSize);

	/** Notifies the parser that no more data will be coming. Returns true if the parser state is valid */
	bool finish();

	/** Returns true if the data parsed so far was valid */
	bool isValid() const { return (mState != psInvalid); }

	/** Returns true if the parser expects no more data */
	bool isFinished() const { return ((mState == psInvalid) || (mState == psFinished)); }


protected:

	enum State
	{
		psKeySpace,        ///< Parsing the space in front of the next key
		psKey,             ///< Currently adding more chars to the key in mCurrentKey
		psEqualSpace,      ///< Space after mCurrentKey
		psEqual,           ///< Just parsed the = sign after a name
		psValueInSQuotes,  ///< Just parsed a Single-quote sign after the Equal sign
		psValueInDQuotes,  ///< Just parsed a Double-quote sign after the Equal sign
		psValueRaw,        ///< Just parsed a raw value without a quote
		psAfterValue,      ///< Just finished parsing the value, waiting for semicolon or data end
		psInvalid,         ///< The parser has encountered an invalid input; further parsing is skipped
		psFinished,        ///< The parser has already been instructed to finish and doesn't expect any more data
	};


	/** The current state of the parser */
	State mState;

	/** If true, the parser will accept keys without an equal sign and the value */
	bool mAllowsKeyOnly;

	/** Buffer for the current Key */
	std::string mCurrentKey;

	/** Buffer for the current Value; */
	std::string mCurrentValue;
} ;





}  // namespace Http
