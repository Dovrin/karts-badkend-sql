// Windows specific macros to ensure correct network headers compatibility
#ifdef _WIN32
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0A00 // Target Windows 10
  #endif
#endif

// Include necessary standard libraries
#include <iostream>  // For console input/output
#include <string>    // For string manipulation
#include <vector>    // For handling dynamic arrays
#include <fstream>   // For file reading operations
// Include third-party/system libraries
#include <mysql/mysql.h> // For interacting with the MySQL database
#include "httplib.h"     // A single-header HTTP server library
#include "json.hpp"      // A single-header JSON parsing library

// Make it easier to refer to nlohmann::json
using json = nlohmann::json;

// --- DATABASE CONFIG ---
// Global constants for connecting to the remote Aiven MySQL database
const char* DB_HOST = "mysql-21935f8c-kartsweb-smukie.g.aivencloud.com";
const char* DB_USER = "avnadmin";
const char* DB_NAME = "defaultdb";
const int DB_PORT = 26081;

// Function to establish a connection to the MySQL database
MYSQL* get_db_connection() {
    // Initialize a MYSQL connection object
    MYSQL* conn = mysql_init(NULL);
    if (!conn) return NULL; // Return null if initialization fails
    
    // Retrieve the database password securely from an environment variable
    const char* db_pass = std::getenv("DB_PASSWORD");
    if (!db_pass) {
        // Log an error and abort connection if the password is not provided
        std::cerr << "CRITICAL: DB_PASSWORD environment variable is NOT SET!" << std::endl;
        return NULL;
    }

    // Configure SSL for the connection using a local CA certificate (ca.pem)
    mysql_ssl_set(conn, NULL, NULL, "./ca.pem", NULL, NULL);
    
    // Attempt to connect to the database with the provided credentials
    if (!mysql_real_connect(conn, DB_HOST, DB_USER, db_pass, DB_NAME, DB_PORT, NULL, 0)) {
        // Output error if the connection attempt fails
        std::cerr << "Connection Error: " << mysql_error(conn) << std::endl;
        return NULL;
    }
    return conn; // Return the established connection
}

int main() {
    // Initialize the HTTP server object
    httplib::Server svr;

    // CORS logic: Handles Cross-Origin Resource Sharing (CORS) preflight requests
    svr.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        // Allow requests from any origin (e.g., frontend running on a different port/domain)
        res.set_header("Access-Control-Allow-Origin", "*");
        // Specify allowed HTTP methods
        res.set_header("Access-Control-Allow-Methods", "POST, GET, PUT, DELETE, OPTIONS"); 
        // Allow the 'Content-Type' header in cross-origin requests
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        
        // If it's a preflight OPTIONS request, return 204 No Content
        if (req.method == "OPTIONS") {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }
        // Let normal requests pass through to specific endpoint handlers
        return httplib::Server::HandlerResponse::Unhandled;
    });

    // 1. LOGIN API Endpoint
    // Handles POST requests to authenticate a user
    svr.Post("/api/login", [](const httplib::Request& req, httplib::Response& res) {
        try {
            // Parse the incoming JSON request body
            auto j_req = json::parse(req.body);
            std::string input_user = j_req.value("username", "");
            std::string input_pass = j_req.value("password", "");

            // Read the hardcoded users from 'users.json' file
            std::ifstream file("users.json");
            if (!file.is_open()) {
                // Return 500 error if users file cannot be read
                res.status = 500;
                res.set_content("{\"status\":\"error\", \"message\":\"users.json missing\"}", "application/json");
                return;
            }

            // Parse the file content into a JSON object
            json users_data;
            file >> users_data;
            file.close();

            // Iterate through the users list to verify credentials
            for (auto& user : users_data) {
                if (user["username"] == input_user && user["password"] == input_pass) {
                    // Return success if credentials match
                    res.set_content("{\"status\":\"success\", \"message\":\"Login Berhasil\"}", "application/json");
                    return;
                }
            }
            // If loop ends without match, return 401 Unauthorized
            res.status = 401;
            res.set_content("{\"status\":\"error\", \"message\":\"Invalid credentials\"}", "application/json");
        } catch (...) {
            // Catch malformed JSON or other parsing errors and return 400 Bad Request
            res.status = 400;
            res.set_content("{\"error\":\"Invalid request\"}", "application/json");
        }
    });

    // 2. GET ANNOUNCEMENTS API Endpoint
    // Fetches a list of announcements from the database
    svr.Get("/api/announcements", [](const httplib::Request& req, httplib::Response& res) {
        // Connect to the DB
        MYSQL* conn = get_db_connection();
        if (!conn) { 
            res.status = 500; 
            res.set_content("{\"error\":\"DB Connection Failed\"}", "application/json");
            return; 
        }

        // Execute queries to grab all announcements
        if (mysql_query(conn, "SELECT id, announcements, description, date, location, urgent FROM announcements")) {
            // If query fails, return a 500 with the mysql error
            res.status = 500;
            res.set_content(mysql_error(conn), "text/plain");
        } else {
            // Extract the result set from the query
            MYSQL_RES* result = mysql_store_result(conn);
            json j_list = json::array(); // JSON array to hold results
            if (result) {
                MYSQL_ROW row;
                // Fetch each row and map columns to a JSON object
                while ((row = mysql_fetch_row(result))) {
                    j_list.push_back({
                        {"id", row[0] ? std::stoi(row[0]) : 0},
                        {"announcements", row[1] ? row[1] : ""},
                        {"description", row[2] ? row[2] : ""},
                        {"date", row[3] ? row[3] : ""},
                        {"location", row[4] ? row[4] : ""},
                        {"urgent", row[5] && std::string(row[5]) == "1"} // Convert bit/boolean 1 to JSON 'true'
                    });
                }
                // Free the query result memory footprint
                mysql_free_result(result);
            }
            // Send the serialized JSON array as response
            res.set_content(j_list.dump(), "application/json");
        }
        // Always close the database connection
        mysql_close(conn);
    });

    // 3. GET EVENTS API Endpoint
    // Fetches list of calendar events from the database
    svr.Get("/api/events", [](const httplib::Request& req, httplib::Response& res) {
        // Secure database connection
        MYSQL* conn = get_db_connection();
        if (!conn) { 
            res.status = 500; 
            res.set_content("{\"error\":\"DB Connection Failed\"}", "application/json");
            return; 
        }

        // Run projection query targeting events
        if (mysql_query(conn, "SELECT id, name, date, location, description FROM events")) {
            res.status = 500;
            res.set_content(mysql_error(conn), "text/plain");
        } else {
            MYSQL_RES* result = mysql_store_result(conn);
            json j_list = json::array();
            if (result) {
                MYSQL_ROW row;
                // Populate JSON response payload dynamically
                while ((row = mysql_fetch_row(result))) {
                    j_list.push_back({
                        {"id", row[0] ? std::stoi(row[0]) : 0},
                        {"name", row[1] ? row[1] : ""},
                        {"date", row[2] ? row[2] : ""},
                        {"location", row[3] ? row[3] : ""},
                        {"description", row[4] ? row[4] : ""}
                    });
                }
                mysql_free_result(result);
            }
            res.set_content(j_list.dump(), "application/json");
        }
        mysql_close(conn);
    });

    // 4. ADD ANNOUNCEMENT API Endpoint
    // Inserts a new announcement into the database
    svr.Post("/api/announcements", [](const httplib::Request& req, httplib::Response& res) {
        try {
            // Get payload from incoming request
            auto j = json::parse(req.body);
            MYSQL* conn = get_db_connection();
            if (!conn) { res.status = 500; return; }

            // Construct SQL INSERT query directly.
            // *NOTE*: This concatenates strings and is vulnerable to SQL Injection, 
            // prepared statements should ideally be used.
            std::string query = "INSERT INTO announcements (announcements, description, date, location, urgent) VALUES ('" 
                                + j.value("announcements", "") + "', '" 
                                + j.value("description", "") + "', '" 
                                + j.value("date", "") + "', '" 
                                + j.value("location", "") + "', " 
                                + std::to_string(j.value("urgent", false) ? 1 : 0) + ")";

            // Run the insertion query
            if (mysql_query(conn, query.c_str())) {
                res.status = 500;
                res.set_content(mysql_error(conn), "text/plain");
            } else {
                // Respond with success flag
                res.set_content("{\"status\":\"success\"}", "application/json");
            }
            mysql_close(conn);
        } catch (...) { res.status = 400; } // Handle parsing exceptions
    });

    // 5. ADD EVENT API Endpoint
    // Inserts a new generic event into the database
    svr.Post("/api/events", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            MYSQL* conn = get_db_connection();
            if (!conn) { res.status = 500; return; }

            // Build INSERT query for 'events' table
            std::string query = "INSERT INTO events (name, date, location, description) VALUES ('" 
                                + j.value("name", "") + "', '" 
                                + j.value("date", "") + "', '" 
                                + j.value("location", "") + "', '" 
                                + j.value("description", "") + "')";

            if (mysql_query(conn, query.c_str())) {
                res.status = 500;
                res.set_content(mysql_error(conn), "text/plain");
            } else {
                res.set_content("{\"status\":\"success\"}", "application/json");
            }
            mysql_close(conn);
        } catch (...) { res.status = 400; }
    });

    // 6. UPDATE ANNOUNCEMENT API Endpoint
    // Modifies an existing announcement identified by an ID in the URL path
    svr.Put(R"(/api/announcements/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            // Extract the announcement ID from the URL path regex match
            int id = std::stoi(req.matches[1]);
            // Parse updated data
            auto j = json::parse(req.body);
            MYSQL* conn = get_db_connection();
            if (!conn) { res.status = 500; return; }

            // Construct UPDATE query. Again, concatenating untrusted strings directly.
            std::string query = "UPDATE announcements SET announcements='" + j.value("announcements", "") + 
                                "', description='" + j.value("description", "") + 
                                "', date='" + j.value("date", "") + 
                                "', location='" + j.value("location", "") + 
                                "', urgent=" + std::to_string(j.value("urgent", false) ? 1 : 0) + 
                                " WHERE id=" + std::to_string(id);

            // Execute the updating query
            if (mysql_query(conn, query.c_str())) {
                res.status = 500; // Returns 500 on database error
            } else {
                res.set_content("{\"status\":\"success\"}", "application/json"); // Indicate success
            }
            mysql_close(conn);
        } catch (...) { res.status = 400; } // Indicates request/parsing flaws
    });

    // 7. UPDATE EVENT API Endpoint
    // Modifies an existing event identified by an ID in the URL path
    svr.Put(R"(/api/events/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            // Extract the numerical event ID
            int id = std::stoi(req.matches[1]);
            auto j = json::parse(req.body);
            MYSQL* conn = get_db_connection();
            if (!conn) { res.status = 500; return; }

            // Prepare the UPDATE logic tied to the extracted event ID
            std::string query = "UPDATE events SET name='" + j.value("name", "") + 
                                "', date='" + j.value("date", "") + 
                                "', location='" + j.value("location", "") + 
                                "', description='" + j.value("description", "") + 
                                "' WHERE id=" + std::to_string(id);

            if (mysql_query(conn, query.c_str())) {
                res.status = 500;
            } else {
                res.set_content("{\"status\":\"success\"}", "application/json");
            }
            mysql_close(conn);
        } catch (...) { res.status = 400; }
    });

    // 8. DELETE ANNOUNCEMENT API Endpoint
    // Removes an announcement by its ID dynamically pulled from the URL
    svr.Delete(R"(/api/announcements/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        // Extract the ID of the announcement intended for deletion
        int id = std::stoi(req.matches[1]);
        MYSQL* conn = get_db_connection();
        if (!conn) { res.status = 500; return; }

        // Compile query to permanently remove matching row
        std::string query = "DELETE FROM announcements WHERE id=" + std::to_string(id);
        if (mysql_query(conn, query.c_str())) {
            res.status = 500;
        } else {
            res.set_content("{\"status\":\"success\"}", "application/json");
        }
        mysql_close(conn);
    });

    // 9. DELETE EVENT API Endpoint
    // Removes a specific event entry by its ID passed via the route parameter
    svr.Delete(R"(/api/events/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        // Evaluate numerical ID parameter
        int id = std::stoi(req.matches[1]);
        MYSQL* conn = get_db_connection();
        if (!conn) { res.status = 500; return; }

        // Execute DELETE operation
        std::string query = "DELETE FROM events WHERE id=" + std::to_string(id);
        if (mysql_query(conn, query.c_str())) {
            res.status = 500;
        } else {
            res.set_content("{\"status\":\"success\"}", "application/json");
        }
        mysql_close(conn);
    });

    // Ascertain listening server port via Environment Variable or default to 8080
    const char* port_env = std::getenv("PORT");
    int port = port_env ? std::stoi(port_env) : 8080;
    
    // Output success indication and listening configuration
    std::cout << "Server starting on http://0.0.0.0:" << port << std::endl;
    // Bind server to all network interfaces on the ascertained port 
    svr.listen("0.0.0.0", port);

    // End application logic explicitly ensuring exit protocol adherence
    return 0;
}