cmake_minimum_required(VERSION 3.10)
project(karts-backend CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Cari Threads (ini biasanya aman)
find_package(Threads REQUIRED)

# 2. Setup Manual MySQL untuk Linux (Debian/Ubuntu)
# Di Linux, file header ada di /usr/include/mysql dan library-nya ada di /usr/lib/x86_64-linux-gnu/
set(MYSQL_INCLUDE_DIR "/usr/include/mysql")
set(MYSQL_LIBRARIES "mysqlclient")

# 3. Buat Executable
add_executable(${PROJECT_NAME} main.cc)

# 4. Link Library
target_link_libraries(${PROJECT_NAME} PRIVATE 
    Threads::Threads
    ${MYSQL_LIBRARIES}
)

# 5. Include Paths
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${MYSQL_INCLUDE_DIR}
)

# 6. Copy ca.pem
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ca.pem")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/ca.pem" DESTINATION "${CMAKE_BINARY_DIR}")
endif()