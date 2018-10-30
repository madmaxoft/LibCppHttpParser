
project (LibCppHttpParser LANGUAGES CXX)

set(LIBSOURCES
	src/EnvelopeParser.cpp
	src/FormParser.cpp
	src/Message.cpp
	src/MessageParser.cpp
	src/MultipartParser.cpp
	src/NameValueParser.cpp
	src/TransferEncodingParser.cpp
	src/Utils.cpp
)

set(LIBHEADERS
	src/EnvelopeParser.hpp
	src/FormParser.hpp
	src/Message.hpp
	src/MessageParser.hpp
	src/MultipartParser.hpp
	src/NameValueParser.hpp
	src/TransferEncodingParser.hpp
	src/Utils.hpp
)

add_library(LibCppHttpParser SHARED ${LIBSOURCES} ${LIBHEADERS})
add_library(LibCppHttpParser-static STATIC ${LIBSOURCES} ${LIBHEADERS})




