# Require at least CMake version 3.10
cmake_minimum_required(VERSION 3.10)
# Define the project name
project(karts-backend)

# Require C++17 standard for modern C++ features
set(CMAKE_CXX_STANDARD 17)
# Enforce the requirement, fail if C++17 is not supported
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Cari Threads (ini biasanya aman)
# Find the Threads package, required for concurrent operations in httplib
find_package(Threads REQUIRED)

# 2. Setup Manual MySQL untuk Linux (Debian/Ubuntu)
# Di Linux, file header ada di /usr/include/mysql dan library-nya ada di /usr/lib/x86_64-linux-gnu/
# Manually specify the include directory for MySQL headers manually
set(MYSQL_INCLUDE_DIR "/usr/include/mysql")
# Set the library name to link against (libmysqlclient.so)
set(MYSQL_LIBRARIES "mysqlclient")

# 3. Buat Executable
# Define the final executable name and its source files
add_executable(${PROJECT_NAME} main.cc)

# 4. Link Library
# Link required libraries to the executable
target_link_libraries(${PROJECT_NAME} PRIVATE 
    Threads::Threads   # For multithreading
    ${MYSQL_LIBRARIES} # For MySQL database connection
)

# 5. Include Paths
# Add directories where the compiler should look for header files (.h, .hpp)
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR} # Include headers from current source folder
    ${MYSQL_INCLUDE_DIR}        # Include MySQL headers
)

# 6. Copy ca.pem
# Check if the SSL certificate 'ca.pem' exists in the source directory
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ca.pem")
    # If it exists, copy it to the build directory so the executable can find it at runtime
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/ca.pem" DESTINATION "${CMAKE_BINARY_DIR}")
endif()