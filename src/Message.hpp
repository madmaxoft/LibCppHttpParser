#pragma once

#include <string>
#include <map>
#include <memory>
#include "EnvelopeParser.hpp"
#include "Utils.hpp"





namespace Http {





/** Base for all HTTP messages.
Provides storage and basic handling for headers.
Note that the headers are stored / compared with their keys lowercased.
Multiple header values are concatenated using commas (RFC 2616 @ 4.2) uppon addition. */
class Message
{
public:

	/** Values of the various HTTP status codes. */
	static const int HTTP_OK = 200;
	static const int HTTP_BAD_REQUEST = 400;
	static const int HTTP_NOT_FOUND = 404;


	/** Signals whether the message is a HTTP request or HTTP response. */
	enum Kind
	{
		mkRequest,
		mkResponse,
	};

	typedef std::map<std::string, std::string> NameValueMap;


	Message(Kind aKind);

	// Force a virtual destructor in all descendants
	virtual ~Message() {}

	/** Adds a header into the internal map of headers.
	The header key is lowercase before processing the header.
	Recognizes special headers: Content-Type and Content-Length.
	Descendants may override to recognize and process other headers. */
	virtual void addHeader(const std::string & aKey, const std::string & aValue);

	/** Returns all the headers within the message.
	The header keys are in lowercase. */
	const NameValueMap & headers() const { return mHeaders; }

	/** If the specified header key is found (case-insensitive), returns the header's value.
	Returns the default when header the key is not found. */
	std::string headerToValue(const std::string & aKey, const std::string & aDefault) const;

	/** Returns the value of the specified header as number. The header key is compared case-insensitive.
	If the specified header key is not found, returns the default.
	If the conversion from the header value to a number fails, returns the default. */
	template <typename T>
	T headerToNumber(const std::string & aKey, T aDefault) const
	{
		auto v = headerToValue(aKey, std::string());
		if (v.empty())
		{
			return aDefault;
		}
		T out;
		if (Utils::stringToInteger(v, out))
		{
			return out;
		}
		return aDefault;
	}

	void setContentType  (const std::string & aContentType);
	void setContentLength(size_t aContentLength);

	const std::string & contentType  () const { return mContentType; }
	size_t              contentLength() const { return mContentLength; }


protected:

	Kind mKind;

	/** Map of headers, with their keys lowercased. */
	NameValueMap mHeaders;

	/** Type of the content; parsed by addHeader(), set directly by setContentLength() */
	std::string mContentType;

	/** Length of the content that is to be received.
	std::string::npos when the object is created.
	Parsed by addHeader() or set directly by setContentLength() */
	size_t mContentLength;
} ;





/** Stores outgoing response headers and serializes them to an HTTP data stream. */
class OutgoingResponse:
	public Message
{
	typedef Message Super;

public:

	OutgoingResponse();

	/** Returns the beginning of a response datastream, containing the specified status code, text, and all
	serialized headers.
	The users should send this, then the actual body of the response. */
	std::string serialize(int aStatusCode, const std::string & StatusText) const;
} ;





/** Serializer for simple outgoing responses - those that have a fixed known status
line, headers, and a short body. */
class SimpleOutgoingResponse
{
public:

	/** Returns HTTP response data that represents the specified parameters.
	This overload provides only the Content-Length header. */
	static std::string serialize(
		int aStatusCode,
		const std::string & aStatusText,
		const std::string & aBody
	);


	/** Returns HTTP response data that represents the specified parameters.
	This overload provides only the Content-Type and Content-Length headers. */
	static std::string serialize(
		int aStatusCode,
		const std::string & aStatusText,
		const std::string & aContentType,
		const std::string & aBody
	);

	/** Returns HTTP response data that represents the specified parameters. */
	static std::string serialize(
		int aStatusCode,
		const std::string & aStatusText,
		const std::map<std::string, std::string> & aHeaders,
		const std::string & aBody
	);
};





/** Provides storage for an incoming HTTP request. */
class IncomingRequest:
	public Message
{
	typedef Message Super;


public:

	/** Base class for anything that can be used as the UserData for the request. */
	class UserData
	{
	public:
		// Force a virtual destructor in descendants:
		virtual ~UserData() {}
	};
	typedef std::shared_ptr<UserData> UserDataPtr;


	/** Creates a new instance of the class, containing the method and URL provided by the client. */
	IncomingRequest(const std::string & aMethod, const std::string & aURL);

	/** Returns the method used in the request */
	const std::string & method() const { return mMethod; }

	/** Returns the entire URL used in the request, including the parameters after '?'. */
	const std::string & url() const { return mURL; }

	/** Returns the path part of the URL (without the parameters after '?'). */
	std::string urlPath() const;

	/** Returns true if the request has had the Auth header present. */
	bool hasAuth() const { return mHasAuth; }

	/** Returns the username that the request presented. Only valid if hasAuth() is true */
	const std::string & authUsername() const { return mAuthUsername; }

	/** Returns the password that the request presented. Only valid if hasAuth() is true */
	const std::string & authPassword() const { return mAuthPassword; }

	bool doesAllowKeepAlive() const { return mAllowKeepAlive; }

	/** Attaches any kind of data to this request, to be later retrieved by userData(). */
	void setUserData(UserDataPtr aUserData) { mUserData = aUserData; }

	/** Returns the data attached to this request by the class client. */
	UserDataPtr userData() { return mUserData; }

	/** Adds the specified header into the internal list of headers.
	Overrides the parent to add recognizing additional headers: auth and keepalive. */
	virtual void addHeader(const std::string & aKey, const std::string & aValue) override;


protected:

	/** Method of the request (GET / PUT / POST / ...) */
	std::string mMethod;

	/** Full URL of the request */
	std::string mURL;

	/** Set to true if the request contains auth data that was understood by the parser */
	bool mHasAuth;

	/** The username used for auth */
	std::string mAuthUsername;

	/** The password used for auth */
	std::string mAuthPassword;

	/** Set to true if the request indicated that it supports keepalives.
	If false, the server will close the connection once the request is finished */
	bool mAllowKeepAlive;

	/** Any data attached to the request by the class client. */
	UserDataPtr mUserData;
};




}  // namespace Http
