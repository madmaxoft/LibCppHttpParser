LibCppHttpParser
================

A C++ library for parsing the HTTP 1.1 protocol in a SAX-like manner. The client code pushes the incoming HTTP data into the parser and the parser calls callbacks upon encountering the logical protocol structure.


Usage
=====

To use the library, use CMake to include the top-level CMakeLists.txt, then add the library to your own executable. The top-level CMakeLists defines two flavors of the library:
  - LibCppHttpParser: shared library
  - LibCppHttpParser-static: static library
Choose whichever version better suits your application needs.

Runtime usage is then as follows:
  1. Create an instance of a callback object that will receive the notifications
  2. Create an instance of the parser object
  3. Push data into the parser object, possibly in multiple chunks over time
  
Example skeleton code:
```cpp
/** Define the callbacks for a single HTTP connection.
This is where most of your code goes, into the overridden handlers. */
class ConnectionCallbacks:
	public Http::MessageParser::Callbacks
{
	/** Called when an error has occured while parsing. */
	virtual void onError(const std::string & aErrorDescription) override { ... };

	/** Called when the first line of the request or response is fully parsed.
	Doesn't check the validity of the line, only extracts the first complete line. */
	virtual void onFirstLine(const std::string & aFirstLine) override { ... };

	/** Called when a single header line is parsed. */
	virtual void onHeaderLine(const std::string & aKey, const std::string & aValue) override { ... };

	/** Called when all the headers have been parsed. */
	virtual void onHeadersFinished(void) override { ... };

	/** Called for each chunk of the incoming body data. */
	virtual void onBodyData(const void * aData, size_t aSize) override { ... };

	/** Called when the entire body has been reported by OnBodyData(). */
	virtual void onBodyFinished(void) override { ... };
};

// Create a parser that uses the connection callbacks:
Http::MessageParser parser(new ConnectionCallbacks);

// Push the data into the parser:
// EXAMPLE 1: Synchronous IO:
while (true)
{
	char buffer[200];
	auto numReceived = receive(socketFd, buffer, sizeof(buffer), 0);
	if (numReceived <= 0)
	{
		// Error or EOF
		break;
	}
	if (parser.parse(buffer, static_cast<size_t>(numReceived)) == std::string::npos)
	{
		// Bad incoming data
		break;
	}
}

// Example 2: Asynchronous IO:
auto handler = [&parser, &socket](const char * aData, size_t aSize)
{
	if (parser.parse(aData, a_Size) == std::string::npos)
	{
		// Bad incoming data
		socket.close();
	}
};
socket.setOnIncomingDataHandler(handler);
// Assumption: the socket object calls the handler whenever new data arrives, asynchronously
```
