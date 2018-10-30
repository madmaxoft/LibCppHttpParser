#pragma once

#include <map>
#include <string>
#include <memory>
#include "MultipartParser.hpp"





namespace Http {





// fwd:
class IncomingRequest;





/** Parses the data sent over HTTP from an HTML form, in a SAX-like form.
The form values are stored within the std::map part of the class, the potentially large file parts are
reported using callbacks.
The user of this class provides callbacks and then pushes data into this class, it then calls the various
callbacks upon encountering the data and stores the simple values within. */
class FormParser:
	public std::map<std::string, std::string>,
	public MultipartParser::Callbacks
{
public:
	enum Kind
	{
		fpkURL,             ///< The form has been transmitted as parameters to a GET request
		fpkFormUrlEncoded,  ///< The form has been POSTed or PUT, with Content-Type of "application/x-www-form-urlencoded"
		fpkMultipart,       ///< The form has been POSTed or PUT, with Content-Type of "multipart/form-data"
	};

	class Callbacks
	{
	public:
		// Force a virtual destructor in descendants:
		virtual ~Callbacks() {}

		/** Called when a new file part is encountered in the form data */
		virtual void onFileStart(FormParser & aParser, const std::string & aFileName) = 0;

		/** Called when more file data has come for the current file in the form data */
		virtual void onFileData(FormParser & aParser, const char * aData, size_t aSize) = 0;

		/** Called when the current file part has ended in the form data */
		virtual void onFileEnd(FormParser & aParser) = 0;
	};


	/** Creates a parser that is tied to a request and notifies of various events using a callback mechanism */
	FormParser(const IncomingRequest & aRequest, Callbacks & aCallbacks);

	/** Creates a parser with the specified content type that reads data from a string */
	FormParser(Kind aKind, const char * aData, size_t aSize, Callbacks & aCallbacks);

	/** Adds more data into the parser, as the request body is received */
	void parse(const char * aData, size_t aSize);

	/** Notifies that there's no more data incoming and the parser should finish its parsing.
	Returns true if parsing successful. */
	bool finish();

	/** Returns true if the headers suggest the request has form data parseable by this class */
	static bool hasFormData(const IncomingRequest & aRequest);


protected:

	/** The callbacks to call for incoming file data */
	Callbacks & mCallbacks;

	/** The kind of the parser (decided in the constructor, used in Parse() */
	Kind mKind;

	/** Buffer for the incoming data until it's parsed */
	std::string mIncomingData;

	/** True if the information received so far is a valid form; set to false on first problem. Further parsing is skipped when false. */
	bool mIsValid;

	/** The parser for the multipart data, if used */
	std::unique_ptr<MultipartParser> mMultipartParser;

	/** Name of the currently parsed part in multipart data */
	std::string mCurrentPartName;

	/** True if the currently parsed part in multipart data is a file */
	bool mIsCurrentPartFile;

	/** Filename of the current parsed part in multipart data (for file uploads) */
	std::string mCurrentPartFileName;

	/** Set to true after mCallbacks.OnFileStart() has been called, reset to false on PartEnd */
	bool mFileHasBeenAnnounced;


	/** Sets up the object for parsing a fpkMultipart request */
	void beginMultipart(const IncomingRequest & aRequest);

	/** Parses mIncomingData as form-urlencoded data (fpkURL or fpkFormUrlEncoded kinds) */
	void parseFormUrlEncoded();

	// cMultipartParser::cCallbacks overrides:
	virtual void onPartStart () override;
	virtual void onPartHeader(const std::string & aKey, const std::string & aValue) override;
	virtual void onPartData  (const char * aData, size_t aSize) override;
	virtual void onPartEnd   () override;
} ;





}  // namespace Http
