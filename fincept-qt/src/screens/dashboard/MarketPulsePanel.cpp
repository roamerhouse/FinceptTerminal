// src/screens/dashboard/MarketPulsePanel.cpp
#include "screens/dashboard/MarketPulsePanel.h"

#include "datahub/DataHub.h"
#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Static Metadata ──────────────────────────────────────────────────────────

static const QStringList kBreadthSymbols = {
    "AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "META", "TSLA", "BRK-B", "JPM", "UNH",
    "V", "XOM", "LLY", "JNJ", "WMT", "MA", "PG", "HD", "CVX", "MRK",
    "^GSPC", "^IXIC", "^NYA", "^VIX"
};

static const QStringList kMoverSymbols = {
    "AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "META", "TSLA", "NFLX", "AMD", "INTC",
    "QCOM", "ADBE", "CSCO", "ORCL", "CRM", "AVGO", "TXN", "PYPL", "MU", "SNPS"
};

static const QStringList kSnapshotSymbols = {
    "^VIX", "^TNX", "DX-Y.NYB", "GC=F", "CL=F", "BTC-USD"
};

// ── Helpers ──────────────────────────────────────────────────────────────────

static QString format_volume(double vol) {
    if (vol >= 1000000)
        return QString("%1M").arg(vol / 1000000.0, 0, 'f', 1);
    if (vol >= 1000)
        return QString("%1K").arg(vol / 1000.0, 0, 'f', 1);
    return QString::number(vol);
}

// ── Constructor ─────────────────────────────────────────────────────────────

MarketPulsePanel::MarketPulsePanel(QWidget* parent) : QWidget(parent) {
    setObjectName("MarketPulsePanel");
    setFixedWidth(280);
    setStyleSheet(QString("background: %1; border-left: 1px solid %2;")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    build_ui();
    hub_subscribe_all();

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        refresh_data();
    });
}

MarketPulsePanel::~MarketPulsePanel() {
    hub_unsubscribe_all();
}

void MarketPulsePanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    hub_subscribe_all();
    refresh_data();
}

void MarketPulsePanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    hub_unsubscribe_all();
}

// ── UI Construction ──────────────────────────────────────────────────────────

void MarketPulsePanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header ──
    auto* header = new QWidget(this);
    header->setFixedHeight(48);
    header->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
                              .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(16, 0, 16, 0);

    auto* title = new QLabel("市场脉动");
    title->setStyleSheet(
        QString("color: %1; font-size: 11px; font-weight: bold; letter-spacing: 1.5px;").arg(ui::colors::AMBER()));
    hl->addWidget(title);
    hl->addStretch();

    auto* live_dot = new QLabel;
    live_dot->setFixedSize(6, 6);
    live_dot->setStyleSheet(QString("background: %1; border-radius: 3px;").arg(ui::colors::POSITIVE()));
    hl->addWidget(live_dot);

    auto* live_text = new QLabel("实时中");
    live_text->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold;").arg(ui::colors::TEXT_SECONDARY()));
    hl->addWidget(live_text);

    root->addWidget(header);

    // ── Scroll Content ──
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: transparent; }");

    auto* container = new QWidget(scroll);
    auto* vl = new QVBoxLayout(container);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_breadth_section());
    vl->addWidget(build_movers_section());
    vl->addWidget(build_global_snapshot_section());
    vl->addWidget(build_market_hours_section());

    vl->addStretch();
    scroll->setWidget(container);
    root->addWidget(scroll);
}

static QWidget* build_section_header(const QString& title, const QChar& icon, const QString& accent) {
    auto* w = new QWidget;
    w->setFixedHeight(32);
    w->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
                         .arg(QString(ui::colors::BG_RAISED()).left(7) + "40", ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(12, 0, 12, 0);

    auto* ilbl = new QLabel(icon);
    ilbl->setStyleSheet(QString("color: %1; font-size: 10px;").arg(accent));
    hl->addWidget(ilbl);

    auto* tlbl = new QLabel(title.toUpper());
    tlbl->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: 800; letter-spacing: 0.5px;")
                            .arg(ui::colors::TEXT_PRIMARY()));
    hl->addWidget(tlbl);
    hl->addStretch();

    return w;
}

QWidget* MarketPulsePanel::build_breadth_section() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("市场宽度", QChar(0x21C4), ui::colors::CYAN()));

    auto* content = new QWidget(this);
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(16, 12, 16, 12);
    cl->setSpacing(8);

    // ── Fear & Greed ──
    auto* fg = new QHBoxLayout;
    auto* fg_lbl = new QLabel("恐贪指数");
    fg_lbl->setStyleSheet(QString("color: %1; font-size: 10px;").arg(ui::colors::TEXT_SECONDARY()));
    fg->addWidget(fg_lbl);
    fg->addStretch();

    fg_score_val_ = new QLabel("--");
    fg->addWidget(fg_score_val_);

    fg_sentiment_ = new QLabel("");
    fg->addWidget(fg_sentiment_);
    cl->addLayout(fg);

    // ── Breadth Bars ──
    auto make_row = [&](const QString& name, BreadthRow& br) {
        auto* row = new QWidget(this);
        auto* rl = new QVBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(2);

        auto* tl = new QHBoxLayout;
        auto* l = new QLabel(name);
        l->setStyleSheet(QString("color: %1; font-size: 9px;").arg(ui::colors::TEXT_TERTIARY()));
        tl->addWidget(l);
        tl->addStretch();
        br.adv = new QLabel("0");
        br.adv->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold;").arg(ui::colors::POSITIVE()));
        tl->addWidget(br.adv);
        auto* sep = new QLabel("/");
        sep->setStyleSheet(QString("color: %1; font-size: 8px;").arg(ui::colors::TEXT_DIM()));
        tl->addWidget(sep);
        br.dec = new QLabel("0");
        br.dec->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold;").arg(ui::colors::NEGATIVE()));
        tl->addWidget(br.dec);
        rl->addLayout(tl);

        auto* bar = new QWidget(this);
        bar->setFixedHeight(4);
        bar->setStyleSheet(QString("background: %1; border-radius: 2px;").arg(ui::colors::BG_BASE()));
        auto* bl = new QHBoxLayout(bar);
        bl->setContentsMargins(0, 0, 0, 0);
        bl->setSpacing(0);
        br.green = new QFrame;
        br.green->setStyleSheet(QString("background: %1; border-radius: 2px;").arg(ui::colors::POSITIVE()));
        br.red = new QFrame;
        br.red->setStyleSheet(QString("background: %1; border-radius: 2px;").arg(ui::colors::NEGATIVE()));
        bl->addWidget(br.green, 1);
        bl->addWidget(br.red, 1);
        rl->addWidget(bar);

        cl->addWidget(row);
    };

    make_row("纽交所 (NYSE)", nyse_row_);
    make_row("纳斯达克 (NASDAQ)", nasdaq_row_);
    make_row("标普 500 (S&P 500)", sp500_row_);

    vl->addWidget(content);
    return w;
}

QWidget* MarketPulsePanel::build_movers_section() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("今日异动", QChar(0x2191), ui::colors::POSITIVE()));

    auto* content = new QWidget(this);
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(12, 8, 12, 12);
    cl->setSpacing(10);

    auto* gainers_hdr = new QLabel("涨幅榜");
    gainers_hdr->setStyleSheet(
        QString("color: %1; font-size: 8px; font-weight: bold; letter-spacing: 0.5px;").arg(ui::colors::POSITIVE()));
    cl->addWidget(gainers_hdr);

    gainers_layout_ = new QVBoxLayout;
    gainers_layout_->setSpacing(4);
    cl->addLayout(gainers_layout_);

    auto* losers_hdr = new QLabel("跌幅榜");
    losers_hdr->setStyleSheet(
        QString("color: %1; font-size: 8px; font-weight: bold; letter-spacing: 0.5px;").arg(ui::colors::NEGATIVE()));
    cl->addWidget(losers_hdr);

    losers_layout_ = new QVBoxLayout;
    losers_layout_->setSpacing(4);
    cl->addLayout(losers_layout_);

    vl->addWidget(content);
    return w;
}

QWidget* MarketPulsePanel::build_mover_row(const QString& sym, double chg, const QString& vol) {
    auto* w = new QWidget(this);
    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(0, 0, 0, 0);

    auto* s = new QLabel(sym);
    s->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold;").arg(ui::colors::TEXT_PRIMARY()));
    hl->addWidget(s);
    hl->addStretch();

    auto* v = new QLabel(vol);
    v->setStyleSheet(QString("color: %1; font-size: 9px;").arg(ui::colors::TEXT_TERTIARY()));
    hl->addWidget(v);

    auto* c = new QLabel(QString("%1%2%").arg(chg >= 0 ? "+" : "").arg(chg, 0, 'f', 2));
    c->setFixedWidth(50);
    c->setAlignment(Qt::AlignRight);
    c->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold;")
                         .arg(chg >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
    hl->addWidget(c);

    return w;
}

QWidget* MarketPulsePanel::build_global_snapshot_section() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("全球概览", QChar(0x25CB), ui::colors::INFO()));

    auto* content = new QWidget(this);
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(12, 6, 12, 6);
    cl->setSpacing(0);

    struct RowDef {
        const char* label;
        StatRow& row;
    };
    RowDef defs[] = {
        {"恐慌指数 (VIX)", vix_row_},     {"美国 10 年期国债", us10y_row_}, {"美元指数", dxy_row_},
        {"黄金价格", gold_row_},   {"原油 (WTI)", oil_row_},  {"比特币", btc_row_},
    };

    for (auto& d : defs) {
        auto* rw = new QWidget(this);
        auto* hl = new QHBoxLayout(rw);
        hl->setContentsMargins(12, 4, 12, 4);

        auto* lbl = new QLabel(d.label);
        hl->addWidget(lbl);
        hl->addStretch();

        d.row.container = rw;
        d.row.name_lbl = lbl;

        d.row.val = new QLabel("--");
        hl->addWidget(d.row.val);

        d.row.chg = new QLabel("");
        hl->addWidget(d.row.chg);

        vl->addWidget(rw);
    }

    vl->addWidget(content);
    return w;
}

QWidget* MarketPulsePanel::build_market_hours_section() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(build_section_header("市场时间", QChar(0x26A1), ui::colors::WARNING()));

    auto* content = new QWidget(this);
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(12, 6, 12, 6);
    cl->setSpacing(0);

    struct ExDef {
        const char* name;
        const char* region;
    };
    ExDef exchanges[] = {
        {"纽交所 / 纳斯达克", "US"}, {"伦交所 (LSE)", "UK"}, {"东京证交所", "JP"}, {"上交所 (SSE)", "CN"}, {"印度证交所 (NSE)", "IN"},
    };

    for (auto& ex : exchanges) {
        auto* row = new QWidget(this);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 3, 0, 3);

        auto* name = new QLabel(ex.name);
        rl->addWidget(name);
        rl->addStretch();

        HoursRow hr;
        hr.container = row;
        hr.name_lbl = name;
        hr.region = ex.region;

        hr.dot = new QLabel;
        hr.dot->setFixedSize(5, 5);
        rl->addWidget(hr.dot);

        hr.status = new QLabel;
        rl->addWidget(hr.status);

        hours_rows_.append(hr);
        cl->addWidget(row);
    }

    vl->addWidget(content);
    return w;
}

// ── Logic ───────────────────────────────────────────────────────────────────

QString MarketPulsePanel::market_status(const QString& region) {
    auto now = QDateTime::currentDateTimeUtc();
    int hour = now.time().hour();
    int day = now.date().dayOfWeek();

    if (day >= 6) return "CLOSED";

    if (region == "US") {
        if (hour >= 13 && hour < 14) return "PRE";
        if (hour >= 14 && hour < 21) return "OPEN";
    } else if (region == "UK") {
        if (hour >= 7 && hour < 8) return "PRE";
        if (hour >= 8 && hour < 17) return "OPEN";
    } else if (region == "JP") {
        if (hour >= 0 && hour < 6) return "OPEN";
    } else if (region == "CN") {
        if (hour >= 1 && hour < 7) return "OPEN";
    } else if (region == "IN") {
        if (hour >= 3 && hour < 10) return "OPEN";
    }
    return "CLOSED";
}

void MarketPulsePanel::refresh_market_hours() {
    for (auto& hr : hours_rows_) {
        QString status = market_status(hr.region);
        QString color = (status == "OPEN")  ? ui::colors::POSITIVE()
                        : (status == "PRE") ? ui::colors::WARNING()
                                             : ui::colors::NEGATIVE();
        hr.dot->setStyleSheet(QString("background: %1; border-radius: 2px;").arg(color));

        QString display_status = (status == "OPEN") ? "开市" : (status == "PRE") ? "盘前" : "休市";
        hr.status->setText(display_status);
        hr.status->setStyleSheet(
            QString("color: %1; font-size: 8px; font-weight: bold; background: transparent;").arg(color));
    }
}

void MarketPulsePanel::rebuild_breadth_from_cache() {
    if (breadth_cache_.isEmpty()) return;

    int sp500_adv = 0, sp500_dec = 0;
    int nasdaq_adv = 0, nasdaq_dec = 0;
    int nyse_adv = 0, nyse_dec = 0;
    double vix = -1;
    int bullish = 0, bearish = 0, neutral_count = 0;

    const QStringList sp500_set = {"AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "META", "TSLA", "BRK-B", "JPM", "UNH", "V", "XOM", "LLY", "JNJ", "WMT", "MA", "PG", "HD", "CVX", "MRK"};
    const QStringList nasdaq_set = {"NFLX", "AMD", "INTC", "QCOM", "ADBE", "CSCO", "ORCL", "CRM", "AVGO", "TXN"};

    for (const auto& sym : kBreadthSymbols) {
        if (!breadth_cache_.contains(sym)) continue;
        const auto& q = breadth_cache_.value(sym);
        if (q.symbol == "^VIX") { vix = q.price; continue; }
        bool in_sp = sp500_set.contains(q.symbol);
        bool in_nq = nasdaq_set.contains(q.symbol);
        if (q.change_pct > 0.3) {
            if (in_sp) ++sp500_adv; else if (in_nq) ++nasdaq_adv; else ++nyse_adv;
            ++bullish;
        } else if (q.change_pct < -0.3) {
            if (in_sp) ++sp500_dec; else if (in_nq) ++nasdaq_dec; else ++nyse_dec;
            ++bearish;
        } else {
            ++neutral_count;
        }
    }

    auto update_row = [](MarketPulsePanel::BreadthRow& row, int adv, int dec) {
        if (!row.adv) return;
        row.adv->setText(QString::number(adv));
        row.dec->setText(QString::number(dec));
        int total = adv + dec;
        if (total == 0) total = 1;
        int adv_pct = static_cast<int>((double(adv) / total) * 100);
        auto* layout = qobject_cast<QHBoxLayout*>(row.green->parentWidget()->layout());
        if (layout) {
            layout->setStretch(0, adv_pct);
            layout->setStretch(1, 100 - adv_pct);
        }
    };
    update_row(nyse_row_, nyse_adv, nyse_dec);
    update_row(nasdaq_row_, nasdaq_adv, nasdaq_dec);
    update_row(sp500_row_, sp500_adv, sp500_dec);

    int total_stocks = bullish + bearish + neutral_count;
    if (total_stocks == 0) total_stocks = 1;
    int score = 50 + static_cast<int>(((bullish - bearish) / static_cast<double>(total_stocks)) * 50);
    if (vix > 0) {
        if (vix > 30) score -= 20; else if (vix > 25) score -= 10; else if (vix < 15) score += 10;
    }
    score = qBound(0, score, 100);

    QString text, color;
    if (score <= 20) { text = "极度恐惧"; color = ui::colors::NEGATIVE(); }
    else if (score <= 40) { text = "恐惧"; color = ui::colors::WARNING(); }
    else if (score <= 60) { text = "中性"; color = ui::colors::WARNING(); }
    else if (score <= 80) { text = "贪婪"; color = ui::colors::POSITIVE(); }
    else { text = "极度贪婪"; color = ui::colors::POSITIVE(); }

    if (fg_score_val_) {
        fg_score_val_->setText(QString::number(score));
        fg_score_val_->setStyleSheet(QString("color: %1; font-size: 18px; font-weight: bold; background: transparent;").arg(color));
    }
    if (fg_sentiment_) {
        fg_sentiment_->setText(text);
        fg_sentiment_->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;").arg(color));
    }
}

void MarketPulsePanel::rebuild_movers_from_cache() {
    if (movers_cache_.isEmpty() || !gainers_layout_ || !losers_layout_) return;

    QVector<services::QuoteData> quotes;
    for (const auto& sym : kMoverSymbols) {
        if (movers_cache_.contains(sym)) quotes.append(movers_cache_.value(sym));
    }
    std::sort(quotes.begin(), quotes.end(), [](const auto& a, const auto& b) { return a.change_pct > b.change_pct; });

    auto clear = [](QVBoxLayout* l) {
        while (l->count()) {
            auto* i = l->takeAt(0);
            if (i->widget()) i->widget()->deleteLater();
            delete i;
        }
    };
    clear(gainers_layout_);
    clear(losers_layout_);

    int g = 0;
    for (const auto& q : quotes) {
        if (q.change_pct <= 0 || g >= 3) break;
        gainers_layout_->addWidget(build_mover_row(q.symbol, q.change_pct, format_volume(q.volume)));
        ++g;
    }
    int l = 0;
    for (int i = quotes.size() - 1; i >= 0 && l < 3; --i) {
        if (quotes[i].change_pct >= 0) continue;
        losers_layout_->addWidget(build_mover_row(quotes[i].symbol, quotes[i].change_pct, format_volume(quotes[i].volume)));
        ++l;
    }
}

void MarketPulsePanel::rebuild_snapshot_from_cache() {
    auto fmt_p = [](const services::QuoteData& q) {
        if (q.price >= 1000) return QString("$%1K").arg(q.price / 1000.0, 0, 'f', 1);
        return QString("%1").arg(q.price, 0, 'f', 2);
    };
    auto fmt_c = [](const services::QuoteData& q) {
        return QString("%1%2%").arg(q.change_pct >= 0 ? "+" : "").arg(q.change_pct, 0, 'f', 2);
    };
    auto up = [&](const QString& s, StatRow& r) {
        if (!snapshot_cache_.contains(s) || !r.val) return;
        const auto& q = snapshot_cache_.value(s);
        r.val->setText(fmt_p(q));
        r.chg->setText(fmt_c(q));
        r.chg->setStyleSheet(QString("color: %1; font-size: 8px; font-weight: bold;").arg(q.change_pct >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
    };
    up("^VIX", vix_row_); up("^TNX", us10y_row_); up("DX-Y.NYB", dxy_row_);
    up("GC=F", gold_row_); up("CL=F", oil_row_); up("BTC-USD", btc_row_);
}

void MarketPulsePanel::hub_subscribe_all() {
    auto& hub = datahub::DataHub::instance();
    QSet<QString> all;
    for (const auto& s : kBreadthSymbols) all.insert(s);
    for (const auto& s : kMoverSymbols) all.insert(s);
    for (const auto& s : kSnapshotSymbols) all.insert(s);

    for (const auto& sym : all) {
        hub.subscribe(this, "market:quote:" + sym, [this, sym](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>()) return;
            const auto q = v.value<services::QuoteData>();
            if (kBreadthSymbols.contains(sym)) {
                breadth_cache_.insert(sym, q); rebuild_breadth_from_cache();
            }
            if (kMoverSymbols.contains(sym)) {
                movers_cache_.insert(sym, q); rebuild_movers_from_cache();
            }
            if (kSnapshotSymbols.contains(sym)) {
                snapshot_cache_.insert(sym, q); rebuild_snapshot_from_cache();
            }
        });
    }
    hub_active_ = true;
}

void MarketPulsePanel::hub_unsubscribe_all() {
    if (hub_active_) { datahub::DataHub::instance().unsubscribe(this); hub_active_ = false; }
}

void MarketPulsePanel::refresh_data() {
    auto& hub = datahub::DataHub::instance();
    QStringList t;
    QSet<QString> s;
    auto p = [&](const QStringList& l) {
        for (const auto& sym : l) { if (!s.contains(sym)) { s.insert(sym); t.append("market:quote:" + sym); } }
    };
    p(kBreadthSymbols); p(kMoverSymbols); p(kSnapshotSymbols);
    hub.request(t);
}

} // namespace fincept::screens
