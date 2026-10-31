#include "osvault/server.hpp"

namespace osvault {

void register_block_routes(crow::SimpleApp& app, ServerContext& ctx) {
    // Get all 1024 blocks
    CROW_ROUTE(app, "/api/blocks")
    ([&ctx]() {
        auto blocks_res = ctx.allocator.get_all_blocks();
        if (blocks_res.is_error()) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = blocks_res.error();
            return crow::response(500, err);
        }

        const auto& blocks = blocks_res.value();
        std::vector<crow::json::wvalue> items;
        items.reserve(blocks.size());

        for (const auto& b : blocks) {
            crow::json::wvalue item;
            item["block_id"] = b.block_id;
            item["is_allocated"] = b.is_allocated;
            item["owner_file_id"] = b.owner_file_id;
            item["owner_file_name"] = b.owner_file_name;
            item["block_index_in_file"] = b.block_index_in_file;
            items.push_back(std::move(item));
        }

        crow::json::wvalue res;
        res["ok"] = true;
        res["total_blocks"] = ctx.allocator.total_blocks();
        res["block_size"] = ctx.storage.block_size();
        res["blocks"] = std::move(items);
        return crow::response(200, res);
    });

    // Get specific block
    CROW_ROUTE(app, "/api/blocks/<int>")
    ([&ctx](int block_id) {
        auto b_res = ctx.allocator.get_block(block_id);
        if (b_res.is_error()) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = b_res.error();
            return crow::response(b_res.code(), err);
        }

        const auto& b = b_res.value();
        crow::json::wvalue res;
        res["ok"] = true;
        res["block_id"] = b.block_id;
        res["is_allocated"] = b.is_allocated;
        res["owner_file_id"] = b.owner_file_id;
        res["owner_file_name"] = b.owner_file_name;
        res["block_index_in_file"] = b.block_index_in_file;
        return crow::response(200, res);
    });
}

} // namespace osvault
