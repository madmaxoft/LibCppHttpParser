#pragma once

#include <string>
#include "EnvelopeParser.hpp"
#include "TransferEncodingParser.hpp"





namespace Http {





/** Parses HTTP messages (request or response) being pushed into the parser, and reports the individual parts
via callbacks. */
class MessageParser:
	protected EnvelopeParser::Callbacks,
	protected TransferEncodingParser::Callbacks
{
public:

	class Callbacks
	{
	public:
		// Force a virtual destructor in descendants:
		virtual ~Callbacks() {}

		/** Called when an error has occured while parsing. */
		virtual void onError(const std::string & aErrorDescription) = 0;

		/** Called when the first line of the request or response is fully parsed.
		Doesn't check the validity of the line, only extracts the first complete line. */
		virtual void onFirstLine(const std::string & aFirstLine) = 0;

		/** Called when a single header line is parsed. */
		virtual void onHeaderLine(const std::string & aKey, const std::string & aValue) = 0;

		/** Called when all the headers have been parsed. */
		virtual void onHeadersFinished() = 0;

		/** Called for each chunk of the incoming body data. */
		virtual void onBodyData(const void * aData, size_t aSize) = 0;

		/** Called when the entire body has been reported by OnBodyData(). */
		virtual void onBodyFinished() = 0;
	};


	/** Creates a new parser instance that will use the specified callbacks for reporting. */
	MessageParser(Callbacks & aCallbacks);

	/** Parses the incoming data and calls the appropriate callbacks.
	Returns the number of bytes consumed or std::string::npos number for error. */
	size_t parse(const char * aData, size_t aSize);

	/** Called when the server indicates no more data will be sent (HTTP 1.0 socket closed).
	Finishes all parsing and calls apropriate callbacks (error if incomplete response). */
	void finish();

	/** Returns true if the entire response has been already parsed. */
	bool isFinished() const { return mIsFinished; }

	/** Resets the parser to the initial state, so that a new request can be parsed. */
	void reset();


protected:

	/** The callbacks used for reporting. */
	Callbacks & mCallbacks;

	/** Set to true if an error has been encountered by the parser. */
	bool mHasHadError;

	/** True if the response has been fully parsed. */
	bool mIsFinished;

	/** The complete first line of the response. Empty if not parsed yet. */
	std::string mFirstLine;

	/** Buffer for the incoming data until the status line is parsed. */
	std::string mBuffer;

	/** Parser for the envelope data (headers) */
	EnvelopeParser mEnvelopeParser;

	/** The specific parser for the transfer encoding used by this response. */
	TransferEncodingParserPtr mTransferEncodingParser;

	/** The transfer encoding to be used by the parser.
	Filled while parsing headers, used when headers are finished. */
	std::string mTransferEncoding;

	/** The content length, parsed from the headers, if available.
	Unused for chunked encoding.
	Filled while parsing headers, used when headers are finished. */
	size_t mContentLength;


	/** Parses the first line out of mBuffer.
	Removes the first line from mBuffer, if appropriate.
	Returns the number of bytes consumed out of mBuffer, or std::string::npos number for error. */
	size_t parseFirstLine();

	/** Parses the message body.
	Processes transfer encoding and calls the callbacks for body data.
	Returns the number of bytes consumed or std::string::npos number for error. */
	size_t parseBody(const char * aData, size_t aSize);

	/** Called internally when the headers-parsing has just finished. */
	void headersFinished();

	// EnvelopeParser::Callbacks overrides:
	virtual void onHeaderLine(const std::string & aKey, const std::string & aValue) override;

	// TransferEncodingParser::Callbacks overrides:
	virtual void onError(const std::string & aErrorDescription) override;
	virtual void onBodyData(const void * aData, size_t aSize) override;
	virtual void onBodyFinished() override;
};





}  // namespace Http
