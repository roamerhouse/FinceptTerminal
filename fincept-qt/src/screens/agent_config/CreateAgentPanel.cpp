#include "CreateAgentPanel.h"
#include <QString>
#include <QLabel>
#include <QCheckBox>
#include <QJsonObject>

#include "ai_chat/LlmService.h"
#include "core/logging/Logger.h"
#include "mcp/McpService.h"
#include "services/agents/AgentService.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "storage/repositories/McpServerRepository.h"
#include "ui/Localization.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QScrollArea>
#include <QShowEvent>
#include <QSplitter>
#include <QUuid>
#include <QVBoxLayout>

namespace {

using namespace fincept::ui::colors;

// ── Shared style helpers ─────────────────────────────────────────────────────

static QString input_style() {
    return QString("background:%1;color:%2;border:1px solid %3;padding:3px 6px;font-size:12px;")
        .arg(BG_RAISED, TEXT_PRIMARY, BORDER_MED);
}

static QString combo_style() {
    return QString("QComboBox{background:%1;color:%2;border:1px solid %3;padding:3px 6px;font-size:12px;}"
                   "QComboBox::drop-down{border:none;}"
                   "QComboBox QAbstractItemView{background:%1;color:%2;selection-background-color:%4;}")
        .arg(BG_RAISED, TEXT_PRIMARY, BORDER_MED, AMBER_DIM);
}

static QString list_style() {
    return QString("QListWidget{background:%1;border:1px solid %2;color:%3;font-size:11px;}"
                   "QListWidget::item{padding:3px 6px;border-bottom:1px solid %2;}"
                   "QListWidget::item:selected{background:%4;color:%3;}"
                   "QListWidget::item:hover{background:%5;}")
        .arg(BG_BASE, BORDER_DIM, TEXT_PRIMARY, AMBER_DIM, BG_HOVER);
}

static QLabel* section_label(const QString& text) {
    auto* l = new QLabel(text);
    l->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;"
                             "padding:8px 0 3px 0;border-bottom:1px solid %2;margin-bottom:2px;")
                         .arg(AMBER, BORDER_DIM));
    return l;
}

static QLabel* field_lbl(const QString& text) {
    auto* l = new QLabel(text);
    l->setStyleSheet(QString("color:%1;font-size:10px;min-width:80px;").arg(TEXT_SECONDARY));
    return l;
}

} // namespace

namespace fincept {
namespace screens {

CreateAgentPanel::CreateAgentPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("CreateAgentPanel");
    build_ui();
    setup_connections();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { update(); });
}

// ── Layout ───────────────────────────────────────────────────────────────────

void CreateAgentPanel::build_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* sp = new QSplitter(Qt::Horizontal);
    sp->setHandleWidth(1);
    sp->setStyleSheet(QString("QSplitter::handle{background:%1;}").arg(ui::colors::BORDER_DIM()));
    sp->addWidget(build_saved_panel());
    sp->addWidget(build_form_panel());
    sp->addWidget(build_test_panel());
    sp->setSizes({220, 480, 320});
    sp->setStretchFactor(0, 0);
    sp->setStretchFactor(1, 1);
    sp->setStretchFactor(2, 0);
    root->addWidget(sp);
}

// ── Left: saved agents ───────────────────────────────────────────────────────

QWidget* CreateAgentPanel::build_saved_panel() {
    auto* p = new QWidget(this);
    p->setMinimumWidth(180);
    p->setMaximumWidth(260);
    p->setStyleSheet(
        QString("background:%1;border-right:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(p);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header bar
    auto* hdr = new QWidget(this);
    hdr->setFixedHeight(34);
    hdr->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(10, 0, 10, 0);
    auto* t = new QLabel("已保存的智能体");
    t->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;").arg(ui::colors::AMBER()));
    hl->addWidget(t);
    hl->addStretch();
    saved_count_ = new QLabel("0");
    saved_count_->setStyleSheet(
        QString("color:%1;font-size:10px;background:%2;padding:1px 6px;").arg(ui::colors::CYAN(), ui::colors::BG_BASE()));
    hl->addWidget(saved_count_);
    vl->addWidget(hdr);

    saved_list_ = new QListWidget;
    saved_list_->setStyleSheet(list_style());
    vl->addWidget(saved_list_, 1);

    // Action bar
    auto* bar = new QWidget(this);
    bar->setFixedHeight(36);
    bar->setStyleSheet(
        QString("background:%1;border-top:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* bl = new QHBoxLayout(bar);
    bl->setContentsMargins(8, 4, 8, 4);
    bl->setSpacing(4);

    auto make_bar_btn = [](const QString& text, const QString& col) -> QPushButton* {
        auto* b = new QPushButton(text);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %1;"
                                 "padding:2px 8px;font-size:9px;font-weight:700;}"
                                 "QPushButton:hover{background:%1;color:%2;}")
                             .arg(col, ui::colors::BG_BASE()));
        return b;
    };

    auto* load_btn = make_bar_btn("加载", ui::colors::AMBER);
    connect(load_btn, &QPushButton::clicked, this, [this]() {
        int row = saved_list_->currentRow();
        if (row >= 0 && row < saved_agents_.size())
            load_agent_into_form(saved_agents_[row]);
    });
    bl->addWidget(load_btn);

    auto* del_btn = make_bar_btn("删除", ui::colors::NEGATIVE);
    connect(del_btn, &QPushButton::clicked, this, &CreateAgentPanel::delete_agent);
    bl->addWidget(del_btn);

    bl->addStretch();

    auto* exp_btn = make_bar_btn("导出", ui::colors::TEXT_SECONDARY);
    connect(exp_btn, &QPushButton::clicked, this, &CreateAgentPanel::export_json);
    bl->addWidget(exp_btn);

    auto* imp_btn = make_bar_btn("导入", ui::colors::TEXT_SECONDARY);
    connect(imp_btn, &QPushButton::clicked, this, &CreateAgentPanel::import_json);
    bl->addWidget(imp_btn);

    vl->addWidget(bar);
    return p;
}

// ── Center: config form ──────────────────────────────────────────────────────

QWidget* CreateAgentPanel::build_form_panel() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QString("QScrollArea{background:%1;border:none;}"
                                  "QScrollBar:vertical{background:%1;width:5px;border:none;}"
                                  "QScrollBar::handle:vertical{background:%2;min-height:20px;}"
                                  "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                              .arg(ui::colors::BG_BASE(), ui::colors::BORDER_BRIGHT()));

    auto* content = new QWidget(this);
    content->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(14, 10, 14, 14);
    vl->setSpacing(3);

    // ── IDENTITY ─────────────────────────────────────────────────────────────
    vl->addWidget(section_label("身份信息 (Identity)"));

    auto* r1 = new QHBoxLayout;
    r1->setSpacing(8);
    r1->addWidget(field_lbl("名称"));
    name_edit_ = new QLineEdit;
    name_edit_->setPlaceholderText("例如：我的股票分析师");
    name_edit_->setStyleSheet(input_style());
    r1->addWidget(name_edit_, 1);
    r1->addWidget(field_lbl("分类"));
    category_combo_ = new QComboBox;
    for (const auto& c : {"general", "trader", "hedge-fund", "economic", "geopolitics", "research", "custom"})
        category_combo_->addItem(c);
    category_combo_->setStyleSheet(combo_style());
    r1->addWidget(category_combo_);
    vl->addLayout(r1);

    desc_edit_ = new QPlainTextEdit;
    desc_edit_->setPlaceholderText("简单描述该智能体的功能...");
    desc_edit_->setFixedHeight(52);
    desc_edit_->setStyleSheet(QString("QPlainTextEdit{%1}").arg(input_style()));
    vl->addWidget(desc_edit_);

    // ── MODEL ────────────────────────────────────────────────────────────────
    vl->addWidget(section_label("模型配置 (Model)"));

    llm_profile_combo_ = new QComboBox;
    llm_profile_combo_->setToolTip("此智能体的 LLM 配置文件。请在 设置 > LLM 配置 中进行配置。");
    llm_profile_combo_->setStyleSheet(combo_style());
    vl->addWidget(llm_profile_combo_);

    llm_resolved_lbl_ = new QLabel;
    llm_resolved_lbl_->setStyleSheet(
        QString("color:%1;font-size:10px;padding:1px 0 4px 0;").arg(ui::colors::TEXT_TERTIARY()));
    vl->addWidget(llm_resolved_lbl_);

    // ── INSTRUCTIONS ─────────────────────────────────────────────────────────
    vl->addWidget(section_label("系统指令 (Instructions)"));

    instructions_edit_ = new QPlainTextEdit;
    instructions_edit_->setPlaceholderText("系统提示词 — 角色、目标、约束、人格...");
    instructions_edit_->setMinimumHeight(110);
    instructions_edit_->setStyleSheet(QString("QPlainTextEdit{%1}").arg(input_style()));
    vl->addWidget(instructions_edit_);

    // ── TOOLS ────────────────────────────────────────────────────────────────
    vl->addWidget(section_label("工具 (Tools)"));

    tool_search_edit_ = new QLineEdit;
    tool_search_edit_->setPlaceholderText("筛选工具...");
    tool_search_edit_->setStyleSheet(input_style());
    vl->addWidget(tool_search_edit_);

    tools_list_ = new QListWidget;
    tools_list_->setFixedHeight(100);
    tools_list_->setStyleSheet(list_style());
    vl->addWidget(tools_list_);

    // ── FEATURES ─────────────────────────────────────────────────────────────
    vl->addWidget(section_label("高级特性 (Features)"));

    // Tag-style checkboxes in a flow grid (2 columns)
    auto make_tag = [](const QString& label) -> QCheckBox* {
        auto* cb = new QCheckBox(label);
        cb->setStyleSheet(QString("QCheckBox{color:%1;font-size:10px;spacing:5px;padding:3px 6px;"
                                  "border:1px solid %2;background:%3;}"
                                  "QCheckBox:checked{color:%4;border-color:%5;background:%6;}"
                                  "QCheckBox::indicator{width:8px;height:8px;}"
                                  "QCheckBox::indicator:unchecked{background:%3;border:1px solid %2;}"
                                  "QCheckBox::indicator:checked{background:%4;border:1px solid %4;}")
                              .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::colors::BG_RAISED(),
                                   ui::colors::AMBER(), ui::colors::AMBER_DIM(), ui::colors::BG_HOVER()));
        return cb;
    };

    struct {
        QCheckBox** ptr;
        const char* label;
    } toggles[] = {
        {&reasoning_check_, "推理 (Reasoning)"},
        {&memory_check_, "记忆 (Memory)"},
        {&knowledge_check_, "知识库 (Knowledge)"},
        {&guardrails_check_, "护栏 (Guardrails)"},
        {&agentic_memory_check_, "智能体记忆"},
        {&storage_check_, "存储 (Storage)"},
        {&tracing_check_, "追踪 (Tracing)"},
        {&compression_check_, "压缩 (Compression)"},
        {&hooks_check_, "钩子 (Hooks)"},
        {&evaluation_check_, "评估 (Evaluation)"},
    };

    auto* tag_row1 = new QHBoxLayout;
    tag_row1->setSpacing(4);
    auto* tag_row2 = new QHBoxLayout;
    tag_row2->setSpacing(4);
    for (int i = 0; i < 10; ++i) {
        *toggles[i].ptr = make_tag(toggles[i].label);
        (i < 5 ? tag_row1 : tag_row2)->addWidget(*toggles[i].ptr);
    }
    tag_row1->addStretch();
    tag_row2->addStretch();
    vl->addLayout(tag_row1);
    vl->addLayout(tag_row2);

    // ── KNOWLEDGE sub ────────────────────────────────────────────────────────
    knowledge_sub_ = new QWidget(this);
    knowledge_sub_->setVisible(false);
    {
        auto* sl = new QVBoxLayout(knowledge_sub_);
        sl->setContentsMargins(0, 4, 0, 0);
        sl->setSpacing(4);
        auto* kr1 = new QHBoxLayout;
        kr1->addWidget(field_lbl("类型"));
        knowledge_type_combo_ = new QComboBox;
        knowledge_type_combo_->addItems({"url", "pdf", "text", "combined"});
        knowledge_type_combo_->setStyleSheet(combo_style());
        kr1->addWidget(knowledge_type_combo_, 1);
        sl->addLayout(kr1);
        knowledge_urls_edit_ = new QPlainTextEdit;
        knowledge_urls_edit_->setPlaceholderText("URL (每行一个)");
        knowledge_urls_edit_->setFixedHeight(52);
        knowledge_urls_edit_->setStyleSheet(QString("QPlainTextEdit{%1}").arg(input_style()));
        sl->addWidget(knowledge_urls_edit_);
        auto* kr2 = new QHBoxLayout;
        kr2->addWidget(field_lbl("向量数据库"));
        knowledge_vectordb_combo_ = new QComboBox;
        knowledge_vectordb_combo_->addItems({"lancedb", "pgvector", "qdrant"});
        knowledge_vectordb_combo_->setStyleSheet(combo_style());
        kr2->addWidget(knowledge_vectordb_combo_, 1);
        kr2->addWidget(field_lbl("嵌入模型"));
        knowledge_embedder_combo_ = new QComboBox;
        knowledge_embedder_combo_->addItems({"openai", "ollama", "sentence-transformers"});
        knowledge_embedder_combo_->setStyleSheet(combo_style());
        kr2->addWidget(knowledge_embedder_combo_, 1);
        sl->addLayout(kr2);
    }
    vl->addWidget(knowledge_sub_);

    // ── GUARDRAILS sub ───────────────────────────────────────────────────────
    guardrails_sub_ = new QWidget(this);
    guardrails_sub_->setVisible(false);
    {
        auto* sl = new QHBoxLayout(guardrails_sub_);
        sl->setContentsMargins(0, 4, 0, 0);
        sl->setSpacing(12);
        auto make_sub_chk = [](const QString& lbl) -> QCheckBox* {
            auto* c = new QCheckBox(lbl);
            c->setStyleSheet(QString("QCheckBox{color:%1;font-size:10px;spacing:5px;}").arg(ui::colors::TEXT_PRIMARY()));
            return c;
        };
        guardrails_pii_check_ = make_sub_chk("PII 隐私检测");
        guardrails_injection_check_ = make_sub_chk("注入攻击防御");
        guardrails_compliance_check_ = make_sub_chk("金融合规检测");
        sl->addWidget(guardrails_pii_check_);
        sl->addWidget(guardrails_injection_check_);
        sl->addWidget(guardrails_compliance_check_);
        sl->addStretch();
    }
    vl->addWidget(guardrails_sub_);

    // ── MEMORY sub ───────────────────────────────────────────────────────────
    memory_sub_ = new QWidget(this);
    memory_sub_->setVisible(false);
    {
        auto* sl = new QVBoxLayout(memory_sub_);
        sl->setContentsMargins(0, 4, 0, 0);
        sl->setSpacing(4);
        auto* mr1 = new QHBoxLayout;
        mr1->addWidget(field_lbl("数据库路径"));
        memory_db_path_edit_ = new QLineEdit;
        memory_db_path_edit_->setPlaceholderText("agent_memory.db");
        memory_db_path_edit_->setStyleSheet(input_style());
        mr1->addWidget(memory_db_path_edit_, 1);
        mr1->addWidget(field_lbl("表名"));
        memory_table_edit_ = new QLineEdit;
        memory_table_edit_->setPlaceholderText("agent_memory");
        memory_table_edit_->setStyleSheet(input_style());
        mr1->addWidget(memory_table_edit_, 1);
        sl->addLayout(mr1);
        auto* mr2 = new QHBoxLayout;
        auto make_sub_chk = [](const QString& lbl) -> QCheckBox* {
            auto* c = new QCheckBox(lbl);
            c->setStyleSheet(QString("QCheckBox{color:%1;font-size:10px;spacing:5px;}").arg(ui::colors::TEXT_PRIMARY()));
            return c;
        };
        memory_user_memories_check_ = make_sub_chk("用户记忆");
        memory_session_summary_check_ = make_sub_chk("会话总结");
        mr2->addWidget(memory_user_memories_check_);
        mr2->addWidget(memory_session_summary_check_);
        mr2->addStretch();
        sl->addLayout(mr2);
    }
    vl->addWidget(memory_sub_);

    // ── STORAGE sub ──────────────────────────────────────────────────────────
    storage_sub_ = new QWidget(this);
    storage_sub_->setVisible(false);
    {
        auto* sl = new QHBoxLayout(storage_sub_);
        sl->setContentsMargins(0, 4, 0, 0);
        sl->setSpacing(8);
        sl->addWidget(field_lbl("类型"));
        storage_type_combo_ = new QComboBox;
        storage_type_combo_->addItems({"sqlite", "postgres"});
        storage_type_combo_->setStyleSheet(combo_style());
        sl->addWidget(storage_type_combo_);
        sl->addWidget(field_lbl("数据库路径"));
        storage_db_path_edit_ = new QLineEdit;
        storage_db_path_edit_->setStyleSheet(input_style());
        sl->addWidget(storage_db_path_edit_, 1);
        sl->addWidget(field_lbl("表名"));
        storage_table_edit_ = new QLineEdit;
        storage_table_edit_->setStyleSheet(input_style());
        sl->addWidget(storage_table_edit_, 1);
    }
    vl->addWidget(storage_sub_);

    // ── AGENTIC MEMORY sub ───────────────────────────────────────────────────
    agentic_memory_sub_ = new QWidget(this);
    agentic_memory_sub_->setVisible(false);
    {
        auto* sl = new QHBoxLayout(agentic_memory_sub_);
        sl->setContentsMargins(0, 4, 0, 0);
        sl->setSpacing(8);
        sl->addWidget(field_lbl("用户 ID"));
        agentic_memory_user_id_edit_ = new QLineEdit;
        agentic_memory_user_id_edit_->setStyleSheet(input_style());
        sl->addWidget(agentic_memory_user_id_edit_, 1);
    }
    vl->addWidget(agentic_memory_sub_);

    // ── MCP SERVERS ──────────────────────────────────────────────────────────
    vl->addWidget(section_label("MCP 服务器"));

    mcp_servers_list_ = new QListWidget;
    mcp_servers_list_->setFixedHeight(72);
    mcp_servers_list_->setSelectionMode(QAbstractItemView::MultiSelection);
    mcp_servers_list_->setStyleSheet(list_style());
    vl->addWidget(mcp_servers_list_);

    // ── ACTIONS ──────────────────────────────────────────────────────────────
    vl->addSpacing(10);
    auto* act = new QHBoxLayout;
    act->setSpacing(6);

    save_btn_ = new QPushButton("保存智能体");
    save_btn_->setCursor(Qt::PointingHandCursor);
    save_btn_->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:none;padding:8px 20px;"
                                     "font-size:11px;font-weight:700;letter-spacing:1px;}"
                                     "QPushButton:hover{background:%3;}")
                                 .arg(ui::colors::AMBER(), ui::colors::BG_BASE(), ui::colors::ORANGE()));
    act->addWidget(save_btn_);

    auto* clr_btn = new QPushButton("清空");
    clr_btn->setCursor(Qt::PointingHandCursor);
    clr_btn->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                                   "padding:8px 14px;font-size:10px;font-weight:600;}"
                                   "QPushButton:hover{background:%3;}")
                               .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER()));
    connect(clr_btn, &QPushButton::clicked, this, &CreateAgentPanel::clear_form);
    act->addWidget(clr_btn);

    act->addStretch();
    vl->addLayout(act);

    status_lbl_ = new QLabel;
    status_lbl_->setStyleSheet(QString("color:%1;font-size:10px;padding:3px 0;").arg(ui::colors::TEXT_TERTIARY()));
    vl->addWidget(status_lbl_);
    vl->addStretch();

    scroll->setWidget(content);
    return scroll;
}

// ── Right: live test panel ───────────────────────────────────────────────────

QWidget* CreateAgentPanel::build_test_panel() {
    auto* p = new QWidget(this);
    p->setMinimumWidth(260);
    p->setMaximumWidth(420);
    p->setStyleSheet(
        QString("background:%1;border-left:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(p);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* hdr = new QWidget(this);
    hdr->setFixedHeight(34);
    hdr->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(10, 0, 10, 0);
    auto* t = new QLabel("实时预览测试");
    t->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;").arg(ui::colors::CYAN()));
    hl->addWidget(t);
    vl->addWidget(hdr);

    // Body
    auto* body = new QWidget(this);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(10, 10, 10, 10);
    bl->setSpacing(6);

    auto* qlbl = new QLabel("查询内容");
    qlbl->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY()));
    bl->addWidget(qlbl);

    test_query_edit_ = new QPlainTextEdit;
    test_query_edit_->setPlaceholderText("输入测试查询内容...");
    test_query_edit_->setFixedHeight(72);
    test_query_edit_->setStyleSheet(
        QString("QPlainTextEdit{background:%1;color:%2;border:1px solid %3;padding:6px;font-size:12px;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED()));
    bl->addWidget(test_query_edit_);

    test_btn_ = new QPushButton("运行测试");
    test_btn_->setCursor(Qt::PointingHandCursor);
    test_btn_->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %1;"
                                     "padding:7px;font-size:10px;font-weight:700;letter-spacing:1px;}"
                                     "QPushButton:hover{background:%1;color:%2;}"
                                     "QPushButton:disabled{color:%3;border-color:%3;}")
                                 .arg(ui::colors::CYAN(), ui::colors::BG_BASE(), ui::colors::TEXT_TERTIARY()));
    connect(test_btn_, &QPushButton::clicked, this, &CreateAgentPanel::test_agent);
    bl->addWidget(test_btn_);

    test_status_lbl_ = new QLabel;
    test_status_lbl_->setStyleSheet(QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY()));
    bl->addWidget(test_status_lbl_);

    auto* rlbl = new QLabel("输出结果");
    rlbl->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;padding-top:4px;")
                            .arg(ui::colors::TEXT_SECONDARY()));
    bl->addWidget(rlbl);

    test_result_ = new QTextEdit;
    test_result_->setReadOnly(true);
    test_result_->setStyleSheet(
        QString("QTextEdit{background:%1;color:%2;border:1px solid %3;padding:8px;font-size:12px;}"
                "QScrollBar:vertical{background:%1;width:5px;border:none;}"
                "QScrollBar::handle:vertical{background:%4;min-height:20px;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
            .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::BORDER_BRIGHT()));
    bl->addWidget(test_result_, 1);

    vl->addWidget(body, 1);
    return p;
}

// ── Connections ──────────────────────────────────────────────────────────────

void CreateAgentPanel::setup_connections() {
    connect(save_btn_, &QPushButton::clicked, this, &CreateAgentPanel::save_agent);

    // Feature toggle → show/hide sub-config panels
    connect(knowledge_check_, &QCheckBox::toggled, knowledge_sub_, &QWidget::setVisible);
    connect(guardrails_check_, &QCheckBox::toggled, guardrails_sub_, &QWidget::setVisible);
    connect(memory_check_, &QCheckBox::toggled, memory_sub_, &QWidget::setVisible);
    connect(storage_check_, &QCheckBox::toggled, storage_sub_, &QWidget::setVisible);
    connect(agentic_memory_check_, &QCheckBox::toggled, agentic_memory_sub_, &QWidget::setVisible);

    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::config_changed, this,
            &CreateAgentPanel::load_profile_combo);
    connect(llm_profile_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &CreateAgentPanel::refresh_llm_pill);
    load_profile_combo();

    auto& svc = services::AgentService::instance();
    connect(&svc, &services::AgentService::config_saved, this, [this]() {
        status_lbl_->setText("保存成功");
        status_lbl_->setStyleSheet(QString("color:%1;font-size:10px;padding:3px 0;").arg(ui::colors::POSITIVE()));
        load_saved_agents();
    });
    connect(&svc, &services::AgentService::config_deleted, this, [this]() {
        status_lbl_->setText("智能体已删除");
        status_lbl_->setStyleSheet(QString("color:%1;font-size:10px;padding:3px 0;").arg(ui::colors::WARNING()));
        editing_id_.clear();
        load_saved_agents();
    });
    connect(&svc, &services::AgentService::agent_stream_thinking, this,
            [this](const QString& request_id, const QString& status) {
                if (request_id != pending_request_id_)
                    return;
                test_status_lbl_->setText(status);
            });
    connect(&svc, &services::AgentService::agent_stream_token, this,
            [this](const QString& request_id, const QString& token) {
                if (request_id != pending_request_id_)
                    return;
                QTextCursor cursor = test_result_->textCursor();
                cursor.movePosition(QTextCursor::End);
                test_result_->setTextCursor(cursor);
                test_result_->insertPlainText(token + " ");
            });
    connect(&svc, &services::AgentService::agent_stream_done, this, [this](services::AgentExecutionResult r) {
        if (r.request_id != pending_request_id_)
            return;
        pending_request_id_.clear();
        test_btn_->setEnabled(true);
        test_btn_->setText("运行测试");
        if (r.success) {
            test_result_->setPlainText(r.response);
            test_status_lbl_->setText(QString("执行完毕，耗时 %1ms").arg(r.execution_time_ms));
            test_status_lbl_->setStyleSheet(
                QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::POSITIVE()));
        } else {
            test_result_->setPlainText("Error: " + r.error);
            test_status_lbl_->setText("失败");
            test_status_lbl_->setStyleSheet(
                QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::NEGATIVE()));
        }
    });
    connect(&svc, &services::AgentService::agent_result, this, [this](services::AgentExecutionResult r) {
        if (r.request_id != pending_request_id_)
            return;
        pending_request_id_.clear();
        test_btn_->setEnabled(true);
        test_btn_->setText("运行测试");
        if (r.success) {
            test_result_->setPlainText(r.response);
            test_status_lbl_->setText(QString("执行完毕，耗时 %1ms").arg(r.execution_time_ms));
            test_status_lbl_->setStyleSheet(
                QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::POSITIVE()));
        } else {
            test_result_->setPlainText("Error: " + r.error);
            test_status_lbl_->setText("失败");
            test_status_lbl_->setStyleSheet(
                QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::NEGATIVE()));
        }
    });
    connect(&svc, &services::AgentService::tools_loaded, this, [this](services::AgentToolsInfo info) {
        available_tools_.clear();
        for (const auto& cat : info.categories) {
            QJsonArray arr = info.tools[cat].toArray();
            for (const auto& t : arr)
                available_tools_.append(t.toString());
        }
        tools_list_->clear();
        for (const auto& t : available_tools_) {
            auto* item = new QListWidgetItem(t);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
            tools_list_->addItem(item);
        }
    });
    connect(tool_search_edit_, &QLineEdit::textChanged, this, [this](const QString& q) {
        for (int i = 0; i < tools_list_->count(); ++i) {
            auto* item = tools_list_->item(i);
            item->setHidden(!q.isEmpty() && !item->text().contains(q, Qt::CaseInsensitive));
        }
    });
    // Double-click in saved list loads immediately
    connect(saved_list_, &QListWidget::itemDoubleClicked, this, [this]() {
        int row = saved_list_->currentRow();
        if (row >= 0 && row < saved_agents_.size())
            load_agent_into_form(saved_agents_[row]);
    });
}

// ── Data helpers ─────────────────────────────────────────────────────────────

void CreateAgentPanel::load_profile_combo() {
    const QString prev_id = llm_profile_combo_->currentData().toString();
    llm_profile_combo_->blockSignals(true);
    llm_profile_combo_->clear();
    llm_profile_combo_->addItem("默认 (全局)", QString{});

    const auto pr = LlmProfileRepository::instance().list_profiles();
    const auto profiles = pr.is_ok() ? pr.value() : QVector<LlmProfile>{};
    for (const auto& p : profiles) {
        QString label = p.name;
        if (p.is_default)
            label += " [默认]";
        llm_profile_combo_->addItem(label, p.id);
    }
    int restore = 0;
    if (!prev_id.isEmpty()) {
        for (int i = 1; i < llm_profile_combo_->count(); ++i) {
            if (llm_profile_combo_->itemData(i).toString() == prev_id) {
                restore = i;
                break;
            }
        }
    }
    llm_profile_combo_->blockSignals(false);
    llm_profile_combo_->setCurrentIndex(restore);
    refresh_llm_pill();
}

void CreateAgentPanel::refresh_llm_pill() {
    const QString profile_id = llm_profile_combo_->currentData().toString();
    ResolvedLlmProfile resolved;
    if (!profile_id.isEmpty()) {
        const auto pr2 = LlmProfileRepository::instance().list_profiles();
        const auto profiles2 = pr2.is_ok() ? pr2.value() : QVector<LlmProfile>{};
        for (const auto& p : profiles2) {
            if (p.id == profile_id) {
                resolved.profile_id = p.id;
                resolved.profile_name = p.name;
                resolved.provider = p.provider;
                resolved.model_id = p.model_id;
                resolved.api_key = p.api_key;
                resolved.base_url = p.base_url;
                resolved.temperature = p.temperature;
                resolved.max_tokens = p.max_tokens;
                resolved.system_prompt = p.system_prompt;
                break;
            }
        }
    }
    if (resolved.provider.isEmpty())
        resolved = ai_chat::LlmService::instance().resolve_profile("agent", editing_id_);

    if (!resolved.provider.isEmpty()) {
        QString text = resolved.provider.toUpper() + " / " + resolved.model_id;
        if (text.length() > 55)
            text = text.left(53) + "..";
        if (profile_id.isEmpty())
            text += " (继承)";
        llm_resolved_lbl_->setText(text);
        llm_resolved_lbl_->setStyleSheet(
            QString("color:%1;font-size:10px;padding:1px 0 4px 0;").arg(ui::colors::TEXT_TERTIARY()));
    } else {
        llm_resolved_lbl_->setText("无模型提供商 — 请在 设置 > LLM 配置 中设置");
        llm_resolved_lbl_->setStyleSheet(
            QString("color:%1;font-size:10px;padding:1px 0 4px 0;").arg(ui::colors::NEGATIVE()));
    }
}

void CreateAgentPanel::load_saved_agents() {
    saved_list_->clear();
    auto result = AgentConfigRepository::instance().list_all();
    if (result.is_ok()) {
        saved_agents_ = result.value();
        for (const auto& a : saved_agents_) {
            QString display = ui::Localization::translate_agent_name(a.name);
            saved_list_->addItem(QString("%1  [%2]").arg(display, a.category));
        }
        saved_count_->setText(QString::number(saved_agents_.size()));
    }
}

void CreateAgentPanel::load_agent_into_form(const AgentConfig& cfg) {
    editing_id_ = cfg.id;

    const QString assigned_id = LlmProfileRepository::instance().get_assignment("agent", cfg.id);
    llm_profile_combo_->blockSignals(true);
    int select_idx = 0;
    if (!assigned_id.isEmpty()) {
        for (int i = 1; i < llm_profile_combo_->count(); ++i) {
            if (llm_profile_combo_->itemData(i).toString() == assigned_id) {
                select_idx = i;
                break;
            }
        }
    }
    llm_profile_combo_->blockSignals(false);
    llm_profile_combo_->setCurrentIndex(select_idx);
    refresh_llm_pill();

    name_edit_->setText(cfg.name);
    desc_edit_->setPlainText(cfg.description);
    int ci = category_combo_->findText(cfg.category);
    if (ci >= 0)
        category_combo_->setCurrentIndex(ci);

    const QJsonObject c = QJsonDocument::fromJson(cfg.config_json.toUtf8()).object();
    instructions_edit_->setPlainText(c["instructions"].toString());

    QJsonArray ta = c["tools"].toArray();
    QSet<QString> selected;
    for (const auto& t : ta)
        selected.insert(t.toString());
    for (int i = 0; i < tools_list_->count(); ++i)
        tools_list_->item(i)->setCheckState(selected.contains(tools_list_->item(i)->text()) ? Qt::Checked
                                                                                            : Qt::Unchecked);

    reasoning_check_->setChecked(c["reasoning"].toBool());
    memory_check_->setChecked(c["memory"].toBool());
    knowledge_check_->setChecked(c["knowledge"].toBool());
    guardrails_check_->setChecked(c["guardrails"].toBool());
    agentic_memory_check_->setChecked(c["agentic_memory"].toBool());
    storage_check_->setChecked(c["storage"].toBool());
    tracing_check_->setChecked(c["tracing"].toBool());
    compression_check_->setChecked(c["compression"].toBool());
    hooks_check_->setChecked(c["hooks"].toBool());
    evaluation_check_->setChecked(c["evaluation"].toBool());

    QJsonObject kc = c["knowledge_config"].toObject();
    int ki = knowledge_type_combo_->findText(kc["type"].toString());
    if (ki >= 0)
        knowledge_type_combo_->setCurrentIndex(ki);
    QStringList url_list;
    for (const auto& u : kc["urls"].toArray())
        url_list.append(u.toString());
    knowledge_urls_edit_->setPlainText(url_list.join("\n"));
    int vi = knowledge_vectordb_combo_->findText(kc["vector_db"].toString());
    if (vi >= 0)
        knowledge_vectordb_combo_->setCurrentIndex(vi);
    int ei = knowledge_embedder_combo_->findText(kc["embedder"].toString());
    if (ei >= 0)
        knowledge_embedder_combo_->setCurrentIndex(ei);

    QJsonObject gc = c["guardrails_config"].toObject();
    guardrails_pii_check_->setChecked(gc["pii_detection"].toBool());
    guardrails_injection_check_->setChecked(gc["injection_check"].toBool());
    guardrails_compliance_check_->setChecked(gc["financial_compliance"].toBool());

    QJsonObject mc = c["memory_config"].toObject();
    memory_db_path_edit_->setText(mc["db_path"].toString());
    memory_table_edit_->setText(mc["table_name"].toString());
    memory_user_memories_check_->setChecked(mc["create_user_memories"].toBool());
    memory_session_summary_check_->setChecked(mc["create_session_summary"].toBool());

    QJsonObject sc = c["storage_config"].toObject();
    int sti = storage_type_combo_->findText(sc["type"].toString());
    if (sti >= 0)
        storage_type_combo_->setCurrentIndex(sti);
    storage_db_path_edit_->setText(sc["db_path"].toString());
    storage_table_edit_->setText(sc["table_name"].toString());

    agentic_memory_user_id_edit_->setText(c["agentic_memory_user_id"].toString());

    status_lbl_->setText(QString("已加载: %1").arg(cfg.name));
    status_lbl_->setStyleSheet(QString("color:%1;font-size:10px;padding:3px 0;").arg(ui::colors::CYAN()));
}

void CreateAgentPanel::clear_form() {
    editing_id_.clear();
    name_edit_->clear();
    desc_edit_->clear();
    category_combo_->setCurrentIndex(0);
    instructions_edit_->clear();
    for (int i = 0; i < tools_list_->count(); ++i)
        tools_list_->item(i)->setCheckState(Qt::Unchecked);
    for (auto* cb : {reasoning_check_, memory_check_, knowledge_check_, guardrails_check_, agentic_memory_check_,
                     storage_check_, tracing_check_, compression_check_, hooks_check_, evaluation_check_})
        cb->setChecked(false);
    knowledge_urls_edit_->clear();
    memory_db_path_edit_->clear();
    memory_table_edit_->clear();
    storage_db_path_edit_->clear();
    storage_table_edit_->clear();
    agentic_memory_user_id_edit_->clear();
    guardrails_pii_check_->setChecked(false);
    guardrails_injection_check_->setChecked(false);
    guardrails_compliance_check_->setChecked(false);
    memory_user_memories_check_->setChecked(false);
    memory_session_summary_check_->setChecked(false);
    test_result_->clear();
    test_status_lbl_->clear();
    status_lbl_->setText("表单已清空");
    status_lbl_->setStyleSheet(QString("color:%1;font-size:10px;padding:3px 0;").arg(ui::colors::TEXT_TERTIARY()));
}

QJsonObject CreateAgentPanel::build_config_json() const {
    QJsonObject config;

    const QString profile_id = llm_profile_combo_->currentData().toString();
    ResolvedLlmProfile resolved;
    if (!profile_id.isEmpty()) {
        const auto pr3 = LlmProfileRepository::instance().list_profiles();
        const auto profiles3 = pr3.is_ok() ? pr3.value() : QVector<LlmProfile>{};
        for (const auto& p : profiles3) {
            if (p.id == profile_id) {
                resolved.profile_id = p.id;
                resolved.profile_name = p.name;
                resolved.provider = p.provider;
                resolved.model_id = p.model_id;
                resolved.api_key = p.api_key;
                resolved.base_url = p.base_url;
                resolved.temperature = p.temperature;
                resolved.max_tokens = p.max_tokens;
                resolved.system_prompt = p.system_prompt;
                break;
            }
        }
    }
    if (resolved.provider.isEmpty())
        resolved = ai_chat::LlmService::instance().resolve_profile("agent", editing_id_);
    config["model"] = ai_chat::LlmService::profile_to_json(resolved);

    config["instructions"] = instructions_edit_->toPlainText();

    QJsonArray tools;
    for (int i = 0; i < tools_list_->count(); ++i)
        if (tools_list_->item(i)->checkState() == Qt::Checked)
            tools.append(tools_list_->item(i)->text());
    config["tools"] = tools;

    config["reasoning"] = reasoning_check_->isChecked();
    config["memory"] = memory_check_->isChecked();
    config["knowledge"] = knowledge_check_->isChecked();
    config["guardrails"] = guardrails_check_->isChecked();
    config["agentic_memory"] = agentic_memory_check_->isChecked();
    config["storage"] = storage_check_->isChecked();
    config["tracing"] = tracing_check_->isChecked();
    config["compression"] = compression_check_->isChecked();
    config["hooks"] = hooks_check_->isChecked();
    config["evaluation"] = evaluation_check_->isChecked();

    if (knowledge_check_->isChecked()) {
        QJsonObject kc;
        kc["type"] = knowledge_type_combo_->currentText();
        QJsonArray urls;
        for (const auto& u : knowledge_urls_edit_->toPlainText().split('\n', Qt::SkipEmptyParts))
            urls.append(u.trimmed());
        kc["urls"] = urls;
        kc["vector_db"] = knowledge_vectordb_combo_->currentText();
        kc["embedder"] = knowledge_embedder_combo_->currentText();
        config["knowledge_config"] = kc;
    }
    if (guardrails_check_->isChecked()) {
        QJsonObject gc;
        gc["pii_detection"] = guardrails_pii_check_->isChecked();
        gc["injection_check"] = guardrails_injection_check_->isChecked();
        gc["financial_compliance"] = guardrails_compliance_check_->isChecked();
        config["guardrails_config"] = gc;
    }
    if (memory_check_->isChecked()) {
        QJsonObject mc;
        mc["db_path"] = memory_db_path_edit_->text();
        mc["table_name"] = memory_table_edit_->text();
        mc["create_user_memories"] = memory_user_memories_check_->isChecked();
        mc["create_session_summary"] = memory_session_summary_check_->isChecked();
        config["memory_config"] = mc;
    }
    if (storage_check_->isChecked()) {
        QJsonObject sc;
        sc["type"] = storage_type_combo_->currentText();
        sc["db_path"] = storage_db_path_edit_->text();
        sc["table_name"] = storage_table_edit_->text();
        config["storage_config"] = sc;
    }
    if (agentic_memory_check_->isChecked())
        config["agentic_memory_user_id"] = agentic_memory_user_id_edit_->text();

    return config;
}

void CreateAgentPanel::save_agent() {
    const QString name = name_edit_->text().trimmed();
    if (name.isEmpty()) {
        status_lbl_->setText("必须填写智能体名称");
        status_lbl_->setStyleSheet(QString("color:%1;font-size:10px;padding:3px 0;").arg(ui::colors::NEGATIVE()));
        return;
    }
    AgentConfig db;
    db.id = editing_id_.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : editing_id_;
    db.name = name;
    db.description = desc_edit_->toPlainText().trimmed();
    db.category = category_combo_->currentText();
    db.config_json = QString::fromUtf8(QJsonDocument(build_config_json()).toJson(QJsonDocument::Compact));
    services::AgentService::instance().save_config(db);
    editing_id_ = db.id;

    const QString profile_id = llm_profile_combo_->currentData().toString();
    auto& repo = LlmProfileRepository::instance();
    if (profile_id.isEmpty())
        repo.remove_assignment("agent", editing_id_);
    else
        repo.assign_profile("agent", editing_id_, profile_id);
}

void CreateAgentPanel::delete_agent() {
    int row = saved_list_->currentRow();
    if (row >= 0 && row < saved_agents_.size())
        services::AgentService::instance().delete_config(saved_agents_[row].id);
}

void CreateAgentPanel::test_agent() {
    const QString query = test_query_edit_->toPlainText().trimmed();
    if (query.isEmpty())
        return;
    test_btn_->setEnabled(false);
    test_btn_->setText("正在运行...");
    test_result_->clear();
    test_status_lbl_->setText("正在运行...");
    test_status_lbl_->setStyleSheet(QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::AMBER()));
    pending_request_id_ = services::AgentService::instance().run_agent_streaming(query, build_config_json());
}

void CreateAgentPanel::export_json() {
    const QString path =
        QFileDialog::getSaveFileName(this, "导出智能体配置", "agent_config.json", "JSON (*.json)");
    if (path.isEmpty())
        return;
    QJsonObject out;
    out["name"] = name_edit_->text();
    out["description"] = desc_edit_->toPlainText();
    out["category"] = category_combo_->currentText();
    out["config"] = build_config_json();
    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
        file.write(QJsonDocument(out).toJson(QJsonDocument::Indented));
    status_lbl_->setText("已导出至: " + path);
    status_lbl_->setStyleSheet(QString("color:%1;font-size:10px;padding:3px 0;").arg(ui::colors::POSITIVE()));
}

void CreateAgentPanel::import_json() {
    const QString path = QFileDialog::getOpenFileName(this, "导入智能体配置", {}, "JSON (*.json)");
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    AgentConfig cfg;
    cfg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    cfg.name = obj["name"].toString();
    cfg.description = obj["description"].toString();
    cfg.category = obj["category"].toString("custom");
    cfg.config_json = QString::fromUtf8(QJsonDocument(obj["config"].toObject()).toJson(QJsonDocument::Compact));
    load_agent_into_form(cfg);
    status_lbl_->setText("已从文件导入成功");
    status_lbl_->setStyleSheet(QString("color:%1;font-size:10px;padding:3px 0;").arg(ui::colors::CYAN()));
}

void CreateAgentPanel::apply_tools_selection(const QStringList& tools) {
    for (int i = 0; i < tools_list_->count(); ++i) {
        auto* item = tools_list_->item(i);
        item->setCheckState(tools.contains(item->text()) ? Qt::Checked : Qt::Unchecked);
    }
}

void CreateAgentPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!data_loaded_) {
        data_loaded_ = true;
        load_saved_agents();
        services::AgentService::instance().list_tools();
        auto servers = McpServerRepository::instance().list_all();
        if (servers.is_ok()) {
            mcp_servers_list_->clear();
            for (const auto& s : servers.value())
                mcp_servers_list_->addItem(s.name);
        }
    }
}

} // namespace screens
} // namespace fincept
