#ifdef _WIN32
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0A00
  #endif
#endif

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <mysql/mysql.h>
#include "httplib.h"
#include "json.hpp"

using json = nlohmann::json;

// --- DATABASE CONFIG ---
const char* DB_HOST = "mysql-21935f8c-kartsweb-smukie.g.aivencloud.com";
const char* DB_USER = "avnadmin";
const char* DB_NAME = "defaultdb";
const int DB_PORT = 26081;

MYSQL* get_db_connection() {
    MYSQL* conn = mysql_init(NULL);
    if (!conn) return NULL;
    
    const char* db_pass = std::getenv("DB_PASSWORD");
    if (!db_pass) {
        std::cerr << "CRITICAL: DB_PASSWORD environment variable is NOT SET!" << std::endl;
        return NULL;
    }

    mysql_ssl_set(conn, NULL, NULL, "./ca.pem", NULL, NULL);
    if (!mysql_real_connect(conn, DB_HOST, DB_USER, db_pass, DB_NAME, DB_PORT, NULL, 0)) {
        std::cerr << "Connection Error: " << mysql_error(conn) << std::endl;
        return NULL;
    }
    return conn;
}

int main() {
    httplib::Server svr;

    // CORS logic
    svr.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "POST, GET, PUT, DELETE, OPTIONS"); 
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        if (req.method == "OPTIONS") {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    // 1. LOGIN
    svr.Post("/api/login", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j_req = json::parse(req.body);
            std::string input_user = j_req.value("username", "");
            std::string input_pass = j_req.value("password", "");

            std::ifstream file("users.json");
            if (!file.is_open()) {
                res.status = 500;
                res.set_content("{\"status\":\"error\", \"message\":\"users.json missing\"}", "application/json");
                return;
            }

            json users_data;
            file >> users_data;
            file.close();

            for (auto& user : users_data) {
                if (user["username"] == input_user && user["password"] == input_pass) {
                    res.set_content("{\"status\":\"success\", \"message\":\"Login Berhasil\"}", "application/json");
                    return;
                }
            }
            res.status = 401;
            res.set_content("{\"status\":\"error\", \"message\":\"Invalid credentials\"}", "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid request\"}", "application/json");
        }
    });

    // 2. GET ANNOUNCEMENTS
    svr.Get("/api/announcements", [](const httplib::Request& req, httplib::Response& res) {
        MYSQL* conn = get_db_connection();
        if (!conn) { 
            res.status = 500; 
            res.set_content("{\"error\":\"DB Connection Failed\"}", "application/json");
            return; 
        }

        if (mysql_query(conn, "SELECT id, announcements, description, date, location, urgent FROM announcements")) {
            res.status = 500;
            res.set_content(mysql_error(conn), "text/plain");
        } else {
            MYSQL_RES* result = mysql_store_result(conn);
            json j_list = json::array();
            if (result) {
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(result))) {
                    j_list.push_back({
                        {"id", row[0] ? std::stoi(row[0]) : 0},
                        {"announcements", row[1] ? row[1] : ""},
                        {"description", row[2] ? row[2] : ""},
                        {"date", row[3] ? row[3] : ""},
                        {"location", row[4] ? row[4] : ""},
                        {"urgent", row[5] && std::string(row[5]) == "1"}
                    });
                }
                mysql_free_result(result);
            }
            res.set_content(j_list.dump(), "application/json");
        }
        mysql_close(conn);
    });

    // 3. GET EVENTS
    svr.Get("/api/events", [](const httplib::Request& req, httplib::Response& res) {
        MYSQL* conn = get_db_connection();
        if (!conn) { 
            res.status = 500; 
            res.set_content("{\"error\":\"DB Connection Failed\"}", "application/json");
            return; 
        }

        if (mysql_query(conn, "SELECT id, name, date, location, description FROM events")) {
            res.status = 500;
            res.set_content(mysql_error(conn), "text/plain");
        } else {
            MYSQL_RES* result = mysql_store_result(conn);
            json j_list = json::array();
            if (result) {
                MYSQL_ROW row;
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

    // 4. ADD ANNOUNCEMENT
    svr.Post("/api/announcements", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            MYSQL* conn = get_db_connection();
            if (!conn) { res.status = 500; return; }

            std::string query = "INSERT INTO announcements (announcements, description, date, location, urgent) VALUES ('" 
                                + j.value("announcements", "") + "', '" 
                                + j.value("description", "") + "', '" 
                                + j.value("date", "") + "', '" 
                                + j.value("location", "") + "', " 
                                + std::to_string(j.value("urgent", false) ? 1 : 0) + ")";

            if (mysql_query(conn, query.c_str())) {
                res.status = 500;
                res.set_content(mysql_error(conn), "text/plain");
            } else {
                res.set_content("{\"status\":\"success\"}", "application/json");
            }
            mysql_close(conn);
        } catch (...) { res.status = 400; }
    });

    // 5. ADD EVENT
    svr.Post("/api/events", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            MYSQL* conn = get_db_connection();
            if (!conn) { res.status = 500; return; }

            std::string query = "INSERT INTO events (id, name, date, location, description) VALUES (" 
                                + std::to_string(j.value("id", 0)) + ", '" 
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

    // 6. UPDATE ANNOUNCEMENT
    svr.Put(R"(/api/announcements/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            auto j = json::parse(req.body);
            MYSQL* conn = get_db_connection();
            if (!conn) { res.status = 500; return; }

            std::string query = "UPDATE announcements SET announcements='" + j.value("announcements", "") + 
                                "', description='" + j.value("description", "") + 
                                "', date='" + j.value("date", "") + 
                                "', location='" + j.value("location", "") + 
                                "', urgent=" + std::to_string(j.value("urgent", false) ? 1 : 0) + 
                                " WHERE id=" + std::to_string(id);

            if (mysql_query(conn, query.c_str())) {
                res.status = 500;
            } else {
                res.set_content("{\"status\":\"success\"}", "application/json");
            }
            mysql_close(conn);
        } catch (...) { res.status = 400; }
    });

    // 7. UPDATE EVENT
    svr.Put(R"(/api/events/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            auto j = json::parse(req.body);
            MYSQL* conn = get_db_connection();
            if (!conn) { res.status = 500; return; }

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

    // 8. DELETE ANNOUNCEMENT
    svr.Delete(R"(/api/announcements/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        MYSQL* conn = get_db_connection();
        if (!conn) { res.status = 500; return; }

        std::string query = "DELETE FROM announcements WHERE id=" + std::to_string(id);
        if (mysql_query(conn, query.c_str())) {
            res.status = 500;
        } else {
            res.set_content("{\"status\":\"success\"}", "application/json");
        }
        mysql_close(conn);
    });

    // 9. DELETE EVENT
    svr.Delete(R"(/api/events/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        MYSQL* conn = get_db_connection();
        if (!conn) { res.status = 500; return; }

        std::string query = "DELETE FROM events WHERE id=" + std::to_string(id);
        if (mysql_query(conn, query.c_str())) {
            res.status = 500;
        } else {
            res.set_content("{\"status\":\"success\"}", "application/json");
        }
        mysql_close(conn);
    });

    const char* port_env = std::getenv("PORT");
    int port = port_env ? std::stoi(port_env) : 8080;
    
    std::cout << "Server starting on http://0.0.0.0:" << port << std::endl;
    svr.listen("0.0.0.0", port);

    return 0;
}