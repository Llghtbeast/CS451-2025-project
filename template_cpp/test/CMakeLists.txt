# Add the message unit test executable
add_executable(message_test message_test.cpp)

# Ensure the test can include header files from the project's src folder
target_include_directories(message_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/include)

# If message.cpp is not compiled into a library, build it into the test executable
# (Adjust the path if the source file has another name or location)
target_sources(message_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/src/message.cpp)

# Set language standard if needed
target_compile_features(message_test PRIVATE cxx_std_17)

# Register the test executable with CTest
add_test(NAME message_test COMMAND message_test)