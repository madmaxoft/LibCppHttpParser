#pragma once

#include <string>





namespace Http {





/** A parser for RFC-822 envelope headers, used both in HTTP and in MIME.
The envelope consists of multiple "Key: Value" lines and a final empty line to signal the end of the headers.
Provides SAX-like parsing for envelopes. The user of this class provides callbacks and then pushes data into
this class, it then calls the various callbacks upon encountering the data.
The class doesn't store all the incoming data, only a portion needed to parse the next bit. */
class EnvelopeParser
{
public:
	class Callbacks
	{
	public:
		// Force a virtual destructor in descendants:
		virtual ~Callbacks() {}

		/** Called when a full header line is parsed */
		virtual void onHeaderLine(const std::string & aKey, const std::string & aValue) = 0;
	};


	EnvelopeParser(Callbacks & aCallbacks);

	/** Parses the incoming data.
	Returns the number of bytes consumed from the input. The bytes not consumed are not part of the envelope header.
	Returns std::string::npos on error
	*/
	size_t parse(const char * aData, size_t aSize);

	/** Makes the parser forget everything parsed so far, so that it can be reused for parsing another datastream */
	void reset();

	/** Returns true if more input is expected for the envelope header */
	bool isInHeaders() const { return mIsInHeaders; }

	/** Sets the IsInHeaders flag; used by cMultipartParser to simplify the parser initial conditions */
	void setIsInHeaders(bool aIsInHeaders) { mIsInHeaders = aIsInHeaders; }


public:

	/** Callbacks to call for the various events */
	Callbacks & mCallbacks;

	/** Set to true while the parser is still parsing the envelope headers. Once set to true, the parser will not consume any more data. */
	bool mIsInHeaders;

	/** Buffer for the incoming data until it is parsed */
	std::string mIncomingData;

	/** Holds the last parsed key; used for line-wrapped values */
	std::string mLastKey;

	/** Holds the last parsed value; used for line-wrapped values */
	std::string mLastValue;


	/** Notifies the callback of the key / value stored in mLastKey / mLastValue, then erases them */
	void notifyLast();

	/** Parses one line of header data. Returns true if successful */
	bool parseLine(const char * aData, size_t aSize);
} ;





}  // namespace Http
