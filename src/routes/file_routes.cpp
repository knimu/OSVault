#include "osvault/server.hpp"

namespace osvault {

static int64_t directory_id_from_request(const crow::request& req) {
    const char* value = req.url_params.get("directory_id");
    if (!value) return 0;
    try { return std::max<int64_t>(0, std::stoll(value)); } catch (...) { return -1; }
}

void register_file_routes(crow::SimpleApp& app, ServerContext& ctx) {
    // List files
    CROW_ROUTE(app, "/api/files")
    ([&ctx](const crow::request& req) {
        std::string query = req.url_params.get("q") ? req.url_params.get("q") : "";
        int64_t directory_id = directory_id_from_request(req);
        if (directory_id < 0) return crow::response(400, "Invalid directory_id.");
        auto files_res = ctx.storage.list_directory(directory_id, query);
        if (files_res.is_error()) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = files_res.error();
            return crow::response(files_res.code(), err);
        }

        crow::json::wvalue res;
        res["ok"] = true;
        std::vector<crow::json::wvalue> file_list;
        for (const auto& f : files_res.value()) {
            crow::json::wvalue item;
            item["id"] = f.id;
            item["name"] = f.name;
            item["directory_id"] = f.directory_id;
            item["size"] = f.size;
            item["block_count"] = f.block_count;
            item["block_ids"] = f.block_ids;
            item["created_at"] = f.created_at;
            item["updated_at"] = f.updated_at;
            file_list.push_back(std::move(item));
        }
        res["files"] = std::move(file_list);
        return crow::response(200, res);
    });

    // Read file
    CROW_ROUTE(app, "/api/files/<string>")
    ([&ctx](const crow::request& req, const std::string& name) {
        int64_t directory_id = directory_id_from_request(req);
        if (directory_id < 0) return crow::response(400, "Invalid directory_id.");
        auto meta_res = ctx.storage.get_file_metadata(name, directory_id);
        if (meta_res.is_error()) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = meta_res.error();
            return crow::response(meta_res.code(), err);
        }

        std::string content;
        std::string cache_key = std::to_string(directory_id) + ":" + name;
        if (!ctx.cache.get(cache_key, content)) {
            auto read_res = ctx.storage.read_file(name, directory_id);
            if (read_res.is_error()) {
                crow::json::wvalue err;
                err["ok"] = false;
                err["error"] = read_res.error();
                return crow::response(read_res.code(), err);
            }
            content = read_res.value();
            ctx.cache.put(cache_key, content);
        }

        const auto& f = meta_res.value();
        crow::json::wvalue res;
        res["ok"] = true;
        res["id"] = f.id;
        res["name"] = f.name;
        res["directory_id"] = f.directory_id;
        res["size"] = f.size;
        res["block_count"] = f.block_count;
        res["block_ids"] = f.block_ids;
        res["created_at"] = f.created_at;
        res["updated_at"] = f.updated_at;
        res["content"] = content;
        return crow::response(200, res);
    });

    // Create file
    CROW_ROUTE(app, "/api/files").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("name")) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = "JSON body with 'name' required.";
            return crow::response(400, err);
        }

        std::string name = json["name"].s();
        std::string content = json.has("content") ? std::string(json["content"].s()) : "";
        int64_t directory_id = json.has("directory_id") ? json["directory_id"].i() : 0;
        if (directory_id < 0) return crow::response(400, "Invalid directory_id.");

        auto cr_res = ctx.storage.create_file(name, content, directory_id);
        if (cr_res.is_error()) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = cr_res.error();
            return crow::response(cr_res.code(), err);
        }

        ctx.cache.put(std::to_string(directory_id) + ":" + name, content);

        const auto& f = cr_res.value();
        crow::json::wvalue res;
        res["ok"] = true;
        res["id"] = f.id;
        res["name"] = f.name;
        res["directory_id"] = f.directory_id;
        res["size"] = f.size;
        res["block_count"] = f.block_count;
        res["block_ids"] = f.block_ids;
        res["created_at"] = f.created_at;
        res["updated_at"] = f.updated_at;
        return crow::response(201, res);
    });

    // Update / Write file
    CROW_ROUTE(app, "/api/files/<string>").methods(crow::HTTPMethod::Put)
    ([&ctx](const crow::request& req, const std::string& name) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("content")) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = "JSON body with 'content' required.";
            return crow::response(400, err);
        }

        std::string content = json["content"].s();
        int64_t directory_id = json.has("directory_id") ? json["directory_id"].i() : 0;
        auto wr_res = ctx.storage.write_file(name, content, directory_id);
        if (wr_res.is_error()) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = wr_res.error();
            return crow::response(wr_res.code(), err);
        }

        ctx.cache.put(std::to_string(directory_id) + ":" + name, content);

        auto meta_res = ctx.storage.get_file_metadata(name, directory_id);
        crow::json::wvalue res;
        res["ok"] = true;
        res["message"] = "File written successfully.";
        if (meta_res.is_ok()) {
            const auto& f = meta_res.value();
            res["size"] = f.size;
            res["block_count"] = f.block_count;
            res["block_ids"] = f.block_ids;
            res["updated_at"] = f.updated_at;
        }
        return crow::response(200, res);
    });

    // Rename file
    CROW_ROUTE(app, "/api/files/<string>/rename").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req, const std::string& old_name) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("new_name")) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = "JSON body with 'new_name' required.";
            return crow::response(400, err);
        }

        std::string new_name = json["new_name"].s();
        int64_t directory_id = json.has("directory_id") ? json["directory_id"].i() : 0;
        auto rn_res = ctx.storage.rename_file(old_name, new_name, directory_id);
        if (rn_res.is_error()) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = rn_res.error();
            return crow::response(rn_res.code(), err);
        }

        ctx.cache.invalidate(std::to_string(directory_id) + ":" + old_name);

        crow::json::wvalue res;
        res["ok"] = true;
        res["message"] = "File renamed successfully.";
        res["old_name"] = old_name;
        res["new_name"] = new_name;
        return crow::response(200, res);
    });

    // Delete file
    CROW_ROUTE(app, "/api/files/<string>").methods(crow::HTTPMethod::Delete)
    ([&ctx](const crow::request& req, const std::string& name) {
        int64_t directory_id = directory_id_from_request(req);
        if (directory_id < 0) return crow::response(400, "Invalid directory_id.");
        auto del_res = ctx.storage.delete_file(name, directory_id);
        if (del_res.is_error()) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = del_res.error();
            return crow::response(del_res.code(), err);
        }

        ctx.cache.invalidate(std::to_string(directory_id) + ":" + name);

        crow::json::wvalue res;
        res["ok"] = true;
        res["message"] = "File deleted successfully and logical blocks released.";
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/directories")
    ([&ctx](const crow::request& req) {
        int64_t parent_id = directory_id_from_request(req);
        if (parent_id < 0) return crow::response(400, "Invalid parent directory_id.");
        auto dirs_res = ctx.storage.list_directories(parent_id);
        if (dirs_res.is_error()) return crow::response(dirs_res.code(), dirs_res.error());
        std::vector<crow::json::wvalue> list;
        for (const auto& dir : dirs_res.value()) {
            crow::json::wvalue item;
            item["id"] = dir.id;
            item["name"] = dir.name;
            item["parent_id"] = dir.parent_id;
            item["created_at"] = dir.created_at;
            list.push_back(std::move(item));
        }
        crow::json::wvalue res;
        res["ok"] = true;
        res["directories"] = std::move(list);
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/directories").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("name")) return crow::response(400, "Required: name.");
        int64_t parent_id = json.has("parent_id") ? json["parent_id"].i() : 0;
        auto dir_res = ctx.storage.create_directory(std::string(json["name"].s()), parent_id);
        if (dir_res.is_error()) return crow::response(dir_res.code(), dir_res.error());
        crow::json::wvalue res;
        res["ok"] = true;
        res["id"] = dir_res.value().id;
        res["name"] = dir_res.value().name;
        res["parent_id"] = dir_res.value().parent_id;
        return crow::response(201, res);
    });

    CROW_ROUTE(app, "/api/directories/<int>").methods(crow::HTTPMethod::Delete)
    ([&ctx](int directory_id) {
        auto del_res = ctx.storage.delete_directory(directory_id);
        if (del_res.is_error()) return crow::response(del_res.code(), del_res.error());
        crow::json::wvalue res;
        res["ok"] = true;
        res["message"] = "Directory deleted.";
        return crow::response(200, res);
    });
}

} // namespace osvault
