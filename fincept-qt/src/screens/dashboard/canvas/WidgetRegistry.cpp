#include "screens/dashboard/canvas/WidgetRegistry.h"

#include "screens/dashboard/widgets/AgentErrorsWidget.h"
#include "screens/dashboard/widgets/BrokerHoldingsWidget.h"
#include "screens/dashboard/widgets/CommoditiesWidget.h"
#include "screens/dashboard/widgets/CryptoTickerWidget.h"
#include "screens/dashboard/widgets/CryptoWidget.h"
#include "screens/dashboard/widgets/EconomicCalendarWidget.h"
#include "screens/dashboard/widgets/ForexWidget.h"
#include "screens/dashboard/widgets/IndicesWidget.h"
#include "screens/dashboard/widgets/MarginUsageWidget.h"
#include "screens/dashboard/widgets/MarketQuoteStripWidget.h"
#include "screens/dashboard/widgets/MarketSentimentWidget.h"
#include "screens/dashboard/widgets/NewsCategoryWidget.h"
#include "screens/dashboard/widgets/NewsWidget.h"
#include "screens/dashboard/widgets/OpenPositionsWidget.h"
#include "screens/dashboard/widgets/OrderBookMiniWidget.h"
#include "screens/dashboard/widgets/PerformanceWidget.h"
#include "screens/dashboard/widgets/PolymarketPriceWidget.h"
#include "screens/dashboard/widgets/PortfolioSummaryWidget.h"
#include "screens/dashboard/widgets/QuickTradeWidget.h"
#include "screens/dashboard/widgets/RecentFilesWidget.h"
#include "screens/dashboard/widgets/RiskMetricsWidget.h"
#include "screens/dashboard/widgets/ScreenerWidget.h"
#include "screens/dashboard/widgets/SectorHeatmapWidget.h"
#include "screens/dashboard/widgets/SparklineStripWidget.h"
#include "screens/dashboard/widgets/StockQuoteWidget.h"
#include "screens/dashboard/widgets/TodayPnLWidget.h"
#include "screens/dashboard/widgets/TopMoversWidget.h"
#include "screens/dashboard/widgets/TradeTapeWidget.h"
#include "screens/dashboard/widgets/VideoPlayerWidget.h"
#include "screens/dashboard/widgets/WatchlistWidget.h"

namespace fincept::screens {

WidgetRegistry& WidgetRegistry::instance() {
    static WidgetRegistry inst;
    return inst;
}

WidgetRegistry::WidgetRegistry() {
    // 12-column grid: default_w, default_h, min_w, min_h
    // Factory receives per-instance config (empty QJsonObject for fresh widgets).
    // Existing widgets ignore it until they opt into configurable behaviour.

    // ── Markets ───────────────────────────────────────────────────────────────
    register_widget({"indices", "市场指数", "市场", "全球主要指数 — 标普500, 纳指, 道指, 罗素2000", 4, 5, 3, 4,
                     [](const QJsonObject&) { return widgets::create_indices_widget(); }});

    register_widget({"forex", "外汇货币对", "市场", "主要外汇对 — 欧元/美元, 英镑/美元, 美元/日元", 4, 4, 2, 3,
                     [](const QJsonObject&) { return widgets::create_forex_widget(); }});

    register_widget({"crypto", "加密货币市场", "市场", "热门加密货币 — 比特币, 以太坊, 币安币, 索拉纳", 4, 4, 2, 3,
                     [](const QJsonObject&) { return widgets::create_crypto_widget(); }});

    register_widget({"commodities", "大宗商品", "市场", "黄金、原油、天然气、铜", 4, 4, 2, 3,
                     [](const QJsonObject&) { return widgets::create_commodities_widget(); }});

    register_widget({"sector_heatmap", "板块热图", "市场", "标普 500 板块表现热图", 6, 5, 3, 4,
                     [](const QJsonObject&) { return new widgets::SectorHeatmapWidget; }});

    register_widget({"top_movers", "涨跌榜", "市场", "今日领涨与领跌品种", 6, 5, 3, 4,
                     [](const QJsonObject&) { return new widgets::TopMoversWidget; }});

    register_widget({"sentiment", "市场情绪", "市场", "恐婪指数、牛熊指标", 4, 4, 2, 3,
                     [](const QJsonObject&) { return new widgets::MarketSentimentWidget; }});

    // ── Research ──────────────────────────────────────────────────────────────
    register_widget({"news", "新闻资讯", "研究", "最新财经新闻头条", 8, 4, 3, 3,
                     [](const QJsonObject&) { return new widgets::NewsWidget; }});

    register_widget({"stock_quote", "股票报价", "研究", "个股详情 — 价格、成交量、图表", 4, 5, 2, 3,
                     [](const QJsonObject& cfg) {
                         const QString sym = cfg.value("symbol").toString("AAPL");
                         return new widgets::StockQuoteWidget(sym);
                     }});

    register_widget({"screener", "股票筛选器", "研究", "根据基本面和技术面筛选股票", 6, 5, 3,
                     4, [](const QJsonObject&) { return new widgets::ScreenerWidget; }});

    register_widget({"econ_calendar", "经济日历", "研究", "即将发布的宏观经济事件和数据", 4, 4, 2, 3,
                     [](const QJsonObject&) { return new widgets::EconomicCalendarWidget; }});

    // ── Portfolio ─────────────────────────────────────────────────────────────
    register_widget({"watchlist", "自选股", "投资组合", "您关注的代码及其实时价格", 6, 4, 2, 3,
                     [](const QJsonObject&) { return new widgets::WatchlistWidget; }});

    register_widget({"performance", "绩效分析", "投资组合", "投资组合损益 — 今日、本周、本月、年初至今", 4, 5, 3, 4,
                     [](const QJsonObject&) { return new widgets::PerformanceWidget; }});

    register_widget({"portfolio_summary", "资产汇总", "投资组合",
                     "持仓概览与资产配置明细", 6, 4, 2, 3,
                     [](const QJsonObject&) { return new widgets::PortfolioSummaryWidget; }});

    register_widget({"risk_metrics", "风险指标", "投资组合", "波动率、贝塔系数、最大回撤、夏普比率", 4, 5, 3,
                     4, [](const QJsonObject&) { return new widgets::RiskMetricsWidget; }});

    // ── Trading ───────────────────────────────────────────────────────────────
    register_widget({"quick_trade", "快速交易", "交易", "加密货币和股票的快速下单入口", 4, 5, 2, 3,
                     [](const QJsonObject&) { return new widgets::QuickTradeWidget; }});

    register_widget({"open_positions", "当前持仓", "交易",
                     "券商账户实时持仓 — 通过齿轮图标选择", 6, 5, 3, 3,
                     [](const QJsonObject& cfg) { return new widgets::OpenPositionsWidget(cfg); }});

    register_widget({"working_orders", "活动订单", "交易",
                     "券商账户待处理订单 — 点击 × 取消", 6, 5, 3, 3,
                     [](const QJsonObject& cfg) { return new widgets::OrderBookMiniWidget(cfg); }});

    register_widget({"margin_usage", "保证金占用", "交易",
                     "券商账户资金 — 可用、已用保证金、总额、占用率", 3, 4, 2, 3,
                     [](const QJsonObject& cfg) { return new widgets::MarginUsageWidget(cfg); }});

    register_widget({"today_pnl", "今日损益", "交易",
                     "券商账户总损益 — 当日、已实现、浮动盈亏", 3, 4, 2, 3,
                     [](const QJsonObject& cfg) { return new widgets::TodayPnLWidget(cfg); }});

    register_widget({"holdings", "长期持仓", "投资组合",
                     "长期券商持仓 — 平均成本、现值、盈亏比例", 6, 5, 3, 3,
                     [](const QJsonObject& cfg) { return new widgets::BrokerHoldingsWidget(cfg); }});

    // ── Tools ────────────────────────────────────────────────────────────────
    register_widget({"video_player", "实时电视 / 直播", "工具",
                     "财经电视 — 彭博、CNBC、路透及自定义流", 4, 5, 3, 4,
                     [](const QJsonObject&) { return widgets::create_video_player_widget(); }});

    register_widget({"recent_files", "最近文件", "工具",
                     "最近保存的文件 — 导出数据、报告、笔记本等", 4, 4, 2, 3,
                     [](const QJsonObject&) { return new widgets::RecentFilesWidget; }});

    register_widget({"quote_strip", "行情显示条", "市场",
                     "可配置实时行情列表 — 通过齿轮图标选择代码", 3, 5, 2, 3,
                     [](const QJsonObject& cfg) { return new widgets::MarketQuoteStripWidget(cfg); }});

    register_widget({"crypto_ticker", "加密行情", "市场",
                     "实时快讯条 — 可配置交易对列表", 3, 5, 2, 3,
                     [](const QJsonObject& cfg) { return new widgets::CryptoTickerWidget(cfg); }});

    register_widget({"polymarket_prices", "预测市场", "市场",
                     "实时预测市场价格 (Polymarket) — 可配置资产列表", 3, 5, 2, 3,
                     [](const QJsonObject& cfg) { return new widgets::PolymarketPriceWidget(cfg); }});

    register_widget({"agent_errors", "Agent 错误", "工具",
                     "最近的 Agent 执行失败记录", 5, 4, 3, 3,
                     [](const QJsonObject& cfg) { return new widgets::AgentErrorsWidget(cfg); }});

    register_widget({"sparklines", "走势图", "市场",
                     "可配置实时走势图", 4, 5, 3, 3,
                     [](const QJsonObject& cfg) { return new widgets::SparklineStripWidget(cfg); }});

    register_widget({"trade_tape", "交易流水", "市场",
                     "加密货币成交流水 — 可配置交易对", 4, 5, 3, 4,
                     [](const QJsonObject& cfg) { return new widgets::TradeTapeWidget(cfg); }});

    register_widget({"news_category", "新闻 — 分类", "研究",
                     "按分类筛选的新闻头条", 5, 5, 3, 3,
                     [](const QJsonObject& cfg) { return new widgets::NewsCategoryWidget(cfg); }});
}

void WidgetRegistry::register_widget(WidgetMeta meta) {
    registry_.insert(meta.type_id, std::move(meta));
}

const WidgetMeta* WidgetRegistry::find(const QString& type_id) const {
    auto it = registry_.find(type_id);
    return (it != registry_.end()) ? &it.value() : nullptr;
}

QVector<WidgetMeta> WidgetRegistry::all() const {
    QVector<WidgetMeta> result;
    result.reserve(registry_.size());
    for (auto it = registry_.cbegin(); it != registry_.cend(); ++it)
        result.append(it.value());
    return result;
}

QVector<WidgetMeta> WidgetRegistry::by_category(const QString& category) const {
    QVector<WidgetMeta> result;
    for (const auto& m : registry_) {
        if (category.isEmpty() || m.category == category)
            result.append(m);
    }
    return result;
}

} // namespace fincept::screens
