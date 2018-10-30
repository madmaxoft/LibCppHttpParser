#pragma once

#include <memory>
#include <string>





namespace Http {





// fwd:
class TransferEncodingParser;
typedef std::shared_ptr<TransferEncodingParser> TransferEncodingParserPtr;





/** Used as both the interface that all the parsers share and the (static) factory creating such parsers.
The parsers convert data from raw incoming stream to a processed steram, based on the Transfer-Encoding. */
class TransferEncodingParser
{
public:

	class Callbacks
	{
	public:
		// Force a virtual destructor in descendants
		virtual ~Callbacks() {}

		/** Called when an error has occured while parsing. */
		virtual void onError(const std::string & aErrorDescription) = 0;

		/** Called for each chunk of the incoming body data. */
		virtual void onBodyData(const void * aData, size_t aSize) = 0;

		/** Called when the entire body has been reported by OnBodyData(). */
		virtual void onBodyFinished() = 0;
	};


	////////////////////////////////////////////////////////////////////////////////
	// The interface:

	// Force a virtual destructor in all descendants
	virtual ~TransferEncodingParser() {}

	/** Parses the incoming data and calls the appropriate callbacks.
	Returns the number of bytes from the end of aData that is already not part of this message (if the parser can detect it).
	Returns std::string::npos on an error. */
	virtual size_t parse(const char * aData, size_t aSize) = 0;

	/** To be called when the stream is terminated from the source (connection closed).
	Flushes any buffers and calls appropriate callbacks. */
	virtual void finish() = 0;


	////////////////////////////////////////////////////////////////////////////////
	// The factory:

	/** Creates a new parser for the specified encoding (case-insensitive).
	If the encoding is not known, returns a nullptr.
	aContentLength is the length of the content, received in a Content-Length header, it is used for
	the Identity encoding, it is ignored for the Chunked encoding. */
	static TransferEncodingParserPtr create(
		Callbacks & aCallbacks,
		const std::string & aTransferEncoding,
		size_t aContentLength
	);


protected:

	/** The callbacks used to report progress. */
	Callbacks & mCallbacks;


	TransferEncodingParser(Callbacks & aCallbacks):
		mCallbacks(aCallbacks)
	{
	}
};





}  // namespace Http
