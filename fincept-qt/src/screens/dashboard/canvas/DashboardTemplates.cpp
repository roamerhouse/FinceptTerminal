#include "screens/dashboard/canvas/DashboardTemplates.h"

namespace fincept::screens {

// Helper: create a GridItem with type + 12-col layout (no instance_id)
static GridItem gi(const char* id, int x, int y, int w, int h, int mw = 2, int mh = 3) {
    GridItem item;
    item.id = id;
    item.cell = {x, y, w, h, mw, mh};
    return item;
}

QVector<DashboardTemplate> all_dashboard_templates() {
    return {

        // ── Portfolio Manager ────────────────────────────────────────────────
        {"portfolio_manager",
         "投资组合管理",
         "持仓、绩效、风险和自选股",
         {
             gi("indices", 0, 0, 4, 5, 3, 4),
             gi("performance", 4, 0, 4, 5, 3, 4),
             gi("risk_metrics", 8, 0, 4, 5, 3, 4),
             gi("portfolio_summary", 0, 5, 6, 4),
             gi("watchlist", 6, 5, 6, 4),
             gi("news", 0, 9, 8, 4),
             gi("econ_calendar", 8, 9, 4, 4),
         }},

        // ── Hedge Fund ────────────────────────────────────────────────────────
        {"hedge_fund",
         "对冲基金",
         "板块热图、筛选器、风险和宏观",
         {
             gi("sector_heatmap", 0, 0, 6, 5, 3, 4),
             gi("screener", 6, 0, 6, 5, 3, 4),
             gi("risk_metrics", 0, 5, 4, 4),
             gi("performance", 4, 5, 4, 4),
             gi("econ_calendar", 8, 5, 4, 4),
             gi("news", 0, 9, 12, 4),
         }},

        // ── Crypto Trader ─────────────────────────────────────────────────────
        {"crypto_trader",
         "加密货币交易员",
         "加密货币价格、快速交易、情绪和涨跌榜",
         {
             gi("crypto", 0, 0, 6, 4),
             gi("top_movers", 6, 0, 6, 5, 3, 4),
             gi("quick_trade", 0, 4, 4, 5),
             gi("watchlist", 4, 4, 8, 5),
             gi("sentiment", 0, 9, 6, 4),
             gi("news", 6, 9, 6, 4),
         }},

        // ── Equity Trader ─────────────────────────────────────────────────────
        {"equity_trader",
         "股票交易员",
         "指数、外汇、商品、筛选器和涨跌榜",
         {
             gi("indices", 0, 0, 4, 5, 3, 4),
             gi("forex", 4, 0, 4, 4),
             gi("commodities", 8, 0, 4, 4),
             gi("stock_quote", 0, 5, 4, 5),
             gi("screener", 4, 4, 8, 5, 3, 4),
             gi("top_movers", 0, 10, 6, 4),
             gi("news", 6, 9, 6, 4),
         }},

        // ── Macro Economist ───────────────────────────────────────────────────
        {"macro_economist",
         "宏观经济学家",
         "经济日历、指数、商品和新闻",
         {
             gi("econ_calendar", 0, 0, 6, 5, 3, 4),
             gi("indices", 6, 0, 6, 4),
             gi("commodities", 6, 4, 6, 4),
             gi("performance", 0, 5, 6, 4),
             gi("news", 0, 9, 8, 5),
             gi("risk_metrics", 8, 8, 4, 5),
         }},

        // ── Geopolitics Analyst ───────────────────────────────────────────────
        {"geopolitics",
         "地缘政治分析师",
         "新闻、情绪、经济日历和筛选器",
         {
             gi("news", 0, 0, 8, 5),
             gi("sentiment", 8, 0, 4, 4),
             gi("econ_calendar", 8, 4, 4, 5),
             gi("screener", 0, 5, 6, 5, 3, 4),
             gi("indices", 6, 5, 6, 4),
         }},
    };
}

} // namespace fincept::screens
