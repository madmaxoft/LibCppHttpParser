#pragma once

#include <string>
#include "EnvelopeParser.hpp"





namespace Http {





/** Implements a SAX-like parser for MIME-encoded messages.
The user of this class provides callbacks, then feeds data into this class, and it calls the various
callbacks upon encountering the data. */
class MultipartParser:
	protected EnvelopeParser::Callbacks
{
public:

	class Callbacks
	{
	public:
		// Force a virtual destructor in descendants:
		virtual ~Callbacks() {}

		/** Called when a new part starts */
		virtual void onPartStart() = 0;

		/** Called when a complete header line is received for a part */
		virtual void onPartHeader(const std::string & aKey, const std::string & aValue) = 0;

		/** Called when body for a part is received */
		virtual void onPartData(const char * aData, size_t aSize) = 0;

		/** Called when the current part ends */
		virtual void onPartEnd() = 0;
	};

	/** Creates the parser, expects to find the boundary in aContentType */
	MultipartParser(const std::string & aContentType, Callbacks & aCallbacks);

	/** Parses more incoming data */
	void parse(const char * aData, size_t aSize);


protected:

	/** The callbacks to call for various parsing events */
	Callbacks & mCallbacks;

	/** True if the data parsed so far is valid; if false, further parsing is skipped */
	bool mIsValid;

	/** Parser for each part's envelope */
	EnvelopeParser mEnvelopeParser;

	/** Buffer for the incoming data until it is parsed */
	std::string mIncomingData;

	/** The boundary, excluding both the initial "--" and the terminating CRLF */
	std::string mBoundary;

	/** Set to true if some data for the current part has already been signalized to mCallbacks. Used for proper CRLF inserting. */
	bool mHasHadData;


	/** Parse one line of incoming data. The CRLF has already been stripped from aData / aSize */
	void parseLine(const char * aData, size_t aSize);

	/** Parse one line of incoming data in the headers section of a part. The CRLF has already been stripped from aData / aSize */
	void parseHeaderLine(const char * aData, size_t aSize);

	// EnvelopeParser::Callbacks overrides:
	virtual void onHeaderLine(const std::string & aKey, const std::string & aValue) override;
} ;





}  // namespace Http
