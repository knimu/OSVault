#include "osvault/server.hpp"

namespace osvault {

void register_file_routes(crow::SimpleApp& app, ServerContext& ctx) {
    // List files
    CROW_ROUTE(app, "/api/files")
    ([&ctx](const crow::request& req) {
        std::string query = req.url_params.get("q") ? req.url_params.get("q") : "";
        auto files_res = ctx.storage.list_files(query);
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
    ([&ctx](const std::string& name) {
        auto meta_res = ctx.storage.get_file_metadata(name);
        if (meta_res.is_error()) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = meta_res.error();
            return crow::response(meta_res.code(), err);
        }

        std::string content;
        if (!ctx.cache.get(name, content)) {
            auto read_res = ctx.storage.read_file(name);
            if (read_res.is_error()) {
                crow::json::wvalue err;
                err["ok"] = false;
                err["error"] = read_res.error();
                return crow::response(read_res.code(), err);
            }
            content = read_res.value();
            ctx.cache.put(name, content);
        }

        const auto& f = meta_res.value();
        crow::json::wvalue res;
        res["ok"] = true;
        res["id"] = f.id;
        res["name"] = f.name;
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

        auto cr_res = ctx.storage.create_file(name, content);
        if (cr_res.is_error()) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = cr_res.error();
            return crow::response(cr_res.code(), err);
        }

        ctx.cache.put(name, content);

        const auto& f = cr_res.value();
        crow::json::wvalue res;
        res["ok"] = true;
        res["id"] = f.id;
        res["name"] = f.name;
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
        auto wr_res = ctx.storage.write_file(name, content);
        if (wr_res.is_error()) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = wr_res.error();
            return crow::response(wr_res.code(), err);
        }

        ctx.cache.put(name, content);

        auto meta_res = ctx.storage.get_file_metadata(name);
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
        auto rn_res = ctx.storage.rename_file(old_name, new_name);
        if (rn_res.is_error()) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = rn_res.error();
            return crow::response(rn_res.code(), err);
        }

        ctx.cache.invalidate(old_name);

        crow::json::wvalue res;
        res["ok"] = true;
        res["message"] = "File renamed successfully.";
        res["old_name"] = old_name;
        res["new_name"] = new_name;
        return crow::response(200, res);
    });

    // Delete file
    CROW_ROUTE(app, "/api/files/<string>").methods(crow::HTTPMethod::Delete)
    ([&ctx](const std::string& name) {
        auto del_res = ctx.storage.delete_file(name);
        if (del_res.is_error()) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = del_res.error();
            return crow::response(del_res.code(), err);
        }

        ctx.cache.invalidate(name);

        crow::json::wvalue res;
        res["ok"] = true;
        res["message"] = "File deleted successfully and logical blocks released.";
        return crow::response(200, res);
    });
}

} // namespace osvault
