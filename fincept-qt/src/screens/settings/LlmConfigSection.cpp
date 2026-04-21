// LlmConfigSection.cpp — LLM provider configuration panel (Qt port)

#include "screens/settings/LlmConfigSection.h"

#include "ai_chat/LlmService.h"
#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include <memory>

namespace fincept::screens {

static constexpr const char* TAG = "LlmConfigSection";

const QStringList LlmConfigSection::KNOWN_PROVIDERS = {"openai",     "anthropic", "gemini", "groq",   "deepseek",
                                                       "openrouter", "minimax",   "ollama", "llama", "fincept"};

QString LlmConfigSection::default_base_url(const QString& provider) {
    const QString p = provider.toLower();
    if (p == "openai")
        return {}; // uses default
    if (p == "anthropic")
        return {};
    if (p == "gemini")
        return {};
    if (p == "groq")
        return {};
    if (p == "deepseek")
        return {};
    if (p == "openrouter")
        return {};
    if (p == "minimax")
        return "https://api.minimax.io/v1";
    if (p == "ollama")
        return "http://localhost:11434";
    if (p == "llama")
        return "http://localhost:8080";
    if (p == "fincept")
        return {}; // endpoints are hardcoded in LlmService, no base_url needed
    return {};
}

QStringList LlmConfigSection::fallback_models(const QString& provider) {
    const QString p = provider.toLower();
    if (p == "openai")
        return {"gpt-4o", "gpt-4o-mini", "gpt-4-turbo", "o3-mini"};
    if (p == "anthropic")
        return {"claude-sonnet-4-5-20250514", "claude-opus-4-5", "claude-3-5-sonnet-20241022",
                "claude-3-haiku-20240307"};
    if (p == "gemini")
        return {"gemini-2.5-flash", "gemini-2.5-pro", "gemini-2.0-flash"};
    if (p == "groq")
        return {"llama-3.3-70b-versatile", "mixtral-8x7b-32768", "gemma2-9b-it"};
    if (p == "deepseek")
        return {"deepseek-chat", "deepseek-reasoner"};
    if (p == "openrouter")
        return {"openai/gpt-4o", "anthropic/claude-sonnet-4-5", "google/gemini-2.5-flash"};
    if (p == "minimax")
        return {"MiniMax-M2.7", "MiniMax-M2.7-highspeed", "MiniMax-M2.5", "MiniMax-M2.5-highspeed"};
    if (p == "ollama")
        return {"llama3:8b", "mistral:7b", "codellama:7b"};
    if (p == "llama")
        return {"llama-3-8b", "mistral-7b", "gemma-7b"};
    if (p == "fincept")
        return {"MiniMax-M2.7", "MiniMax-M2.7-highspeed", "MiniMax-M2.5", "MiniMax-M2.5-highspeed"};
    return {};
}

// ============================================================================
// Construction
// ============================================================================

LlmConfigSection::LlmConfigSection(QWidget* parent) : QWidget(parent) {
    build_ui();

    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::models_fetched, this,
            &LlmConfigSection::on_models_fetched);

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        setStyleSheet(QString("background:%1;color:%2;").arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY()));
    });
    setStyleSheet(QString("background:%1;color:%2;").arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY()));

    load_providers();
    load_profiles();
}

void LlmConfigSection::reload() {
    load_providers();
    load_profiles();
}

void LlmConfigSection::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Title bar
    auto* title_bar = new QWidget(this);
    title_bar->setFixedHeight(44);
    title_bar->setStyleSheet("background:" + QString(ui::colors::BG_SURFACE()) + ";border-bottom:1px solid " +
                             QString(ui::colors::BORDER_DIM()) + ";");
    auto* tbl = new QHBoxLayout(title_bar);
    tbl->setContentsMargins(16, 0, 16, 0);
    auto* title_lbl = new QLabel("模型服务商配置");
    title_lbl->setStyleSheet("color:" + QString(ui::colors::AMBER()) + ";font-weight:700;letter-spacing:1px;");
    tbl->addWidget(title_lbl);
    tbl->addStretch();
    root->addWidget(title_bar);

    tab_widget_ = new QTabWidget;
    tab_widget_->setDocumentMode(true);
    tab_widget_->setStyleSheet("QTabWidget::pane{border:none;background:" + QString(ui::colors::BG_BASE()) +
                               ";}"
                               "QTabBar::tab{background:" +
                               QString(ui::colors::BG_SURFACE()) + ";color:" + QString(ui::colors::TEXT_SECONDARY()) +
                               ";padding:8px 20px;"
                               "font-weight:600;letter-spacing:1px;border-bottom:2px solid transparent;}"
                               "QTabBar::tab:selected{color:" +
                               QString(ui::colors::AMBER()) + ";border-bottom:2px solid " +
                               QString(ui::colors::AMBER()) +
                               ";}"
                               "QTabBar::tab:hover{color:" +
                               QString(ui::colors::TEXT_PRIMARY()) + ";}");
    tab_widget_->addTab(build_providers_tab(), "服务商");
    tab_widget_->addTab(build_profiles_tab(), "预设方案");
    root->addWidget(tab_widget_, 1);
}

QWidget* LlmConfigSection::build_providers_tab() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);
    splitter->addWidget(build_provider_list_panel());
    splitter->addWidget(build_form_panel());
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({220, 600});
    vl->addWidget(splitter, 1);

    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet("background:" + QString(ui::colors::BORDER_DIM()) + ";");
    vl->addWidget(sep);
    vl->addWidget(build_global_panel());
    return w;
}

QWidget* LlmConfigSection::build_provider_list_panel() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(220);
    panel->setStyleSheet("background:" + QString(ui::colors::BG_BASE()) + ";border-right:1px solid " +
                         QString(ui::colors::BORDER_DIM()) + ";");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    auto* lbl = new QLabel("服务商列表");
    lbl->setStyleSheet("color:" + QString(ui::colors::TEXT_SECONDARY()) + ";font-weight:700;letter-spacing:1px;");
    vl->addWidget(lbl);

    provider_list_ = new QListWidget;
    provider_list_->setStyleSheet(
        "QListWidget{background:transparent;border:none;color:" + QString(ui::colors::TEXT_PRIMARY()) +
        ";}"
        "QListWidget::item{padding:8px 6px;border-radius:3px;}"
        "QListWidget::item:selected{background:" +
        QString(ui::colors::BG_RAISED()) + ";color:" + QString(ui::colors::AMBER()) +
        ";}"
        "QListWidget::item:hover{background:" +
        QString(ui::colors::BG_RAISED()) + ";}");
    connect(provider_list_, &QListWidget::currentRowChanged, this, &LlmConfigSection::on_provider_selected);
    vl->addWidget(provider_list_, 1);

    auto* btn_row = new QHBoxLayout;
    add_btn_ = new QPushButton("+ 添加");
    add_btn_->setStyleSheet("QPushButton{background:" + QString(ui::colors::BG_RAISED()) + ";color:" +
                            QString(ui::colors::AMBER()) + ";border:1px solid " + QString(ui::colors::AMBER()) +
                            ";"
                            "border-radius:3px;padding:5px 10px;font-weight:600;}"
                            "QPushButton:hover{background:" +
                            QString(ui::colors::BG_RAISED()) + ";}");
    connect(add_btn_, &QPushButton::clicked, this, [this]() {
        // Show input dialog to pick provider
        QStringList choices = KNOWN_PROVIDERS;
        bool ok;
        QString provider = QInputDialog::getItem(this, "添加服务商", "选择服务商:", choices, 0, false, &ok);
        if (!ok || provider.isEmpty())
            return;

        // Check if already exists
        auto existing = LlmConfigRepository::instance().list_providers();
        if (existing.is_ok()) {
            for (const auto& p : existing.value()) {
                if (p.provider.toLower() == provider.toLower()) {
                    show_status("服务商已配置", true);
                    return;
                }
            }
        }

        // Create new empty config
        LlmConfig cfg;
        cfg.provider = provider.toLower();
        cfg.model = "";
        cfg.is_active = false;
        LlmConfigRepository::instance().save_provider(cfg);
        load_providers();

        // Select newly added provider
        for (int i = 0; i < provider_list_->count(); ++i) {
            if (provider_list_->item(i)->data(Qt::UserRole).toString() == cfg.provider) {
                provider_list_->setCurrentRow(i);
                break;
            }
        }
    });

    delete_btn_ = new QPushButton("移除");
    delete_btn_->setEnabled(false);
    delete_btn_->setStyleSheet(
        "QPushButton{background:" + QString(ui::colors::BG_RAISED()) + ";color:" + QString(ui::colors::NEGATIVE()) +
        ";border:1px solid " + QString(ui::colors::BG_RAISED()) +
        ";"
        "border-radius:3px;padding:5px 10px;}"
        "QPushButton:hover{background:" +
        QString(ui::colors::BG_RAISED()) +
        ";}"
        "QPushButton:disabled{color:" +
        QString(ui::colors::BORDER_BRIGHT()) + ";border-color:" + QString(ui::colors::BORDER_MED()) + ";}");
    connect(delete_btn_, &QPushButton::clicked, this, &LlmConfigSection::on_delete_provider);

    btn_row->addWidget(add_btn_);
    btn_row->addWidget(delete_btn_);
    vl->addLayout(btn_row);

    return panel;
}

QWidget* LlmConfigSection::build_form_panel() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea{background:" + QString(ui::colors::BG_BASE()) +
                          ";border:none;}"
                          "QScrollBar:vertical{background:" +
                          QString(ui::colors::BG_RAISED()) +
                          ";width:6px;}"
                          "QScrollBar::handle:vertical{background:" +
                          QString(ui::colors::BORDER_BRIGHT()) +
                          ";border-radius:3px;}"
                          "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");

    auto* page = new QWidget(this);
    page->setStyleSheet("background:" + QString(ui::colors::BG_BASE()) + ";");
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 20, 24, 20);
    vl->setSpacing(14);

    // Section title
    auto* form_title = new QLabel("服务商详细配置");
    form_title->setStyleSheet("color:" + QString(ui::colors::AMBER()) + ";font-weight:700;");
    vl->addWidget(form_title);

    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet("background:" + QString(ui::colors::BORDER_DIM()) + ";");
    vl->addWidget(sep);

    auto* form = new QFormLayout;
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto field_style = [](QLineEdit* le) {
        le->setStyleSheet("QLineEdit{background:" + QString(ui::colors::BG_RAISED()) +
                          ";color:" + QString(ui::colors::TEXT_PRIMARY()) + ";border:1px solid " +
                          QString(ui::colors::BORDER_MED()) +
                          ";"
                          "border-radius:3px;padding:6px;}"
                          "QLineEdit:focus{border:1px solid " +
                          QString(ui::colors::AMBER()) + ";}");
        le->setMinimumWidth(320);
    };
    auto lbl_style = [](QLabel* l) { l->setStyleSheet("color:" + QString(ui::colors::TEXT_SECONDARY()) + ";"); };

    auto* p_lbl = new QLabel("服务商类型");
    lbl_style(p_lbl);
    provider_edit_ = new QLineEdit;
    provider_edit_->setPlaceholderText("例如: openai");
    provider_edit_->setReadOnly(true); // set by selection
    field_style(provider_edit_);
    provider_edit_->setStyleSheet(provider_edit_->styleSheet() +
                                  "QLineEdit{background:" + QString(ui::colors::BG_SURFACE()) +
                                  ";color:" + QString(ui::colors::TEXT_TERTIARY()) + ";}");
    form->addRow(p_lbl, provider_edit_);

    auto* k_lbl = new QLabel("API 凭据 (Key)");
    lbl_style(k_lbl);
    api_key_edit_ = new QLineEdit;
    api_key_edit_->setPlaceholderText("sk-...");
    api_key_edit_->setEchoMode(QLineEdit::Password);
    field_style(api_key_edit_);
    form->addRow(k_lbl, api_key_edit_);

    auto* m_lbl = new QLabel("模型名称");
    lbl_style(m_lbl);
    auto* model_row = new QHBoxLayout;
    model_combo_ = new QComboBox;
    model_combo_->setEditable(true);
    model_combo_->setMinimumWidth(260);
    model_combo_->lineEdit()->setPlaceholderText("选择或输入模型名称...");
    model_combo_->setStyleSheet("QComboBox{background:" + QString(ui::colors::BG_RAISED()) +
                                ";color:" + QString(ui::colors::TEXT_PRIMARY()) + ";border:1px solid " +
                                QString(ui::colors::BORDER_MED()) +
                                ";"
                                "border-radius:3px;padding:6px;}"
                                "QComboBox:focus{border:1px solid " +
                                QString(ui::colors::AMBER()) +
                                ";}"
                                "QComboBox::drop-down{border:none;width:20px;}"
                                "QComboBox::down-arrow{image:none;border-left:4px solid transparent;"
                                "border-right:4px solid transparent;border-top:5px solid " +
                                QString(ui::colors::TEXT_SECONDARY()) +
                                ";}"
                                "QComboBox QAbstractItemView{background:" +
                                QString(ui::colors::BG_RAISED()) + ";color:" + QString(ui::colors::TEXT_PRIMARY()) +
                                ";"
                                "selection-background-color:" +
                                QString(ui::colors::BG_RAISED()) + ";selection-color:" + QString(ui::colors::AMBER()) +
                                ";"
                                "border:1px solid " +
                                QString(ui::colors::BORDER_MED()) + ";}");
    model_row->addWidget(model_combo_, 1);

    fetch_btn_ = new QPushButton("获取列表");
    fetch_btn_->setFixedHeight(30);
    fetch_btn_->setFixedWidth(60);
    fetch_btn_->setStyleSheet(
        "QPushButton{background:" + QString(ui::colors::BG_RAISED()) + ";color:" + QString(ui::colors::AMBER()) +
        ";border:1px solid " + QString(ui::colors::BORDER_BRIGHT()) +
        ";"
        "border-radius:3px;font-weight:600;}"
        "QPushButton:hover{background:" +
        QString(ui::colors::BG_RAISED()) +
        ";}"
        "QPushButton:disabled{color:" +
        QString(ui::colors::TEXT_TERTIARY()) + ";border-color:" + QString(ui::colors::BORDER_MED()) + ";}");
    connect(fetch_btn_, &QPushButton::clicked, this, &LlmConfigSection::on_fetch_models);
    model_row->addWidget(fetch_btn_);

    form->addRow(m_lbl, model_row);

    auto* b_lbl = new QLabel("代理地址 (Base URL)");
    lbl_style(b_lbl);
    base_url_edit_ = new QLineEdit;
    base_url_edit_->setPlaceholderText("可选 — 留空则使用默认地址");
    field_style(base_url_edit_);
    form->addRow(b_lbl, base_url_edit_);

    // Tools toggle
    tools_check_ = new QCheckBox("启用 MCP 工具 (支持自动导航、数据调取、订单管理等)");
    tools_check_->setChecked(true);
    tools_check_->setStyleSheet("QCheckBox{color:" + QString(ui::colors::TEXT_PRIMARY()) +
                                ";spacing:8px;}"
                                "QCheckBox::indicator{width:16px;height:16px;border:1px solid " +
                                QString(ui::colors::BORDER_MED()) +
                                ";border-radius:3px;background:" + QString(ui::colors::BG_RAISED()) +
                                ";}"
                                "QCheckBox::indicator:checked{background:" +
                                QString(ui::colors::AMBER()) + ";border-color:" + QString(ui::colors::AMBER()) + ";}");
    tools_check_->setToolTip("启用后，AI 可以与终端交互：切换界面、获取行情、管理自选等。");
    form->addRow(new QLabel(""), tools_check_);

    vl->addLayout(form);

    // Buttons
    auto* btn_row = new QHBoxLayout;
    save_btn_ = new QPushButton("保存并设为当前引擎");
    save_btn_->setFixedHeight(34);
    save_btn_->setStyleSheet(
        "QPushButton{background:" + QString(ui::colors::AMBER()) + ";color:" + QString(ui::colors::BG_BASE()) +
        ";border:none;border-radius:4px;"
        "font-weight:700;padding:0 16px;}"
        "QPushButton:hover{background:" +
        QString(ui::colors::AMBER_DIM()) +
        ";}"
        "QPushButton:disabled{background:" +
        QString(ui::colors::BORDER_BRIGHT()) + ";color:" + QString(ui::colors::TEXT_TERTIARY()) + ";}");
    connect(save_btn_, &QPushButton::clicked, this, &LlmConfigSection::on_save_provider);

    test_btn_ = new QPushButton("测试连接");
    test_btn_->setFixedHeight(34);
    test_btn_->setStyleSheet("QPushButton{background:" + QString(ui::colors::BG_RAISED()) +
                             ";color:" + QString(ui::colors::TEXT_PRIMARY()) + ";border:1px solid " +
                             QString(ui::colors::BORDER_BRIGHT()) +
                             ";"
                             "border-radius:4px;padding:0 16px;}"
                             "QPushButton:hover{background:" +
                             QString(ui::colors::BG_HOVER()) + ";}");
    connect(test_btn_, &QPushButton::clicked, this, &LlmConfigSection::on_test_connection);

    btn_row->addWidget(save_btn_);
    btn_row->addWidget(test_btn_);
    btn_row->addStretch();
    vl->addLayout(btn_row);

    status_lbl_ = new QLabel;
    status_lbl_->setStyleSheet("");
    status_lbl_->hide();
    vl->addWidget(status_lbl_);

    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget* LlmConfigSection::build_global_panel() {
    auto* panel = new QWidget(this);
    panel->setStyleSheet("background:" + QString(ui::colors::BG_SURFACE()) + ";");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(24, 12, 24, 12);
    vl->setSpacing(10);

    auto* title = new QLabel("全局通用配置");
    title->setStyleSheet("color:" + QString(ui::colors::AMBER()) + ";font-weight:700;letter-spacing:1px;");
    vl->addWidget(title);

    auto* row = new QHBoxLayout;
    row->setSpacing(24);

    // Temperature
    auto* temp_grp = new QVBoxLayout;
    auto* temp_lbl = new QLabel("温度 (Temperature)");
    temp_lbl->setStyleSheet("color:" + QString(ui::colors::TEXT_SECONDARY()) + ";");
    temp_spin_ = new QDoubleSpinBox;
    temp_spin_->setRange(0.0, 2.0);
    temp_spin_->setSingleStep(0.05);
    temp_spin_->setValue(0.7);
    temp_spin_->setDecimals(2);
    temp_spin_->setFixedWidth(100);
    temp_spin_->setStyleSheet("QDoubleSpinBox{background:" + QString(ui::colors::BG_RAISED()) +
                              ";color:" + QString(ui::colors::TEXT_PRIMARY()) + ";border:1px solid " +
                              QString(ui::colors::BORDER_MED()) +
                              ";"
                              "border-radius:3px;padding:4px;}");
    temp_grp->addWidget(temp_lbl);
    temp_grp->addWidget(temp_spin_);
    row->addLayout(temp_grp);

    // Max tokens
    auto* tok_grp = new QVBoxLayout;
    auto* tok_lbl = new QLabel("最大 Token 限制");
    tok_lbl->setStyleSheet("color:" + QString(ui::colors::TEXT_SECONDARY()) + ";");
    tokens_spin_ = new QSpinBox;
    tokens_spin_->setRange(100, 32000);
    tokens_spin_->setSingleStep(100);
    tokens_spin_->setValue(4096);
    tokens_spin_->setFixedWidth(100);
    tokens_spin_->setStyleSheet("QSpinBox{background:" + QString(ui::colors::BG_RAISED()) +
                                ";color:" + QString(ui::colors::TEXT_PRIMARY()) + ";border:1px solid " +
                                QString(ui::colors::BORDER_MED()) +
                                ";"
                                "border-radius:3px;padding:4px;}");
    tok_grp->addWidget(tok_lbl);
    tok_grp->addWidget(tokens_spin_);
    row->addLayout(tok_grp);

    // System prompt
    auto* sp_grp = new QVBoxLayout;
    auto* sp_lbl = new QLabel("全局系统提示词 (System Prompt)");
    sp_lbl->setStyleSheet("color:" + QString(ui::colors::TEXT_SECONDARY()) + ";");
    system_prompt_ = new QPlainTextEdit;
    system_prompt_->setPlaceholderText("为 LLM 设置可选的全局系统提示词...");
    system_prompt_->setFixedHeight(60);
    system_prompt_->setStyleSheet("QPlainTextEdit{background:" + QString(ui::colors::BG_RAISED()) +
                                  ";color:" + QString(ui::colors::TEXT_PRIMARY()) + ";border:1px solid " +
                                  QString(ui::colors::BORDER_MED()) +
                                  ";"
                                  "border-radius:3px;padding:4px;}"
                                  "QPlainTextEdit:focus{border:1px solid " +
                                  QString(ui::colors::AMBER()) + ";}");
    sp_grp->addWidget(sp_lbl);
    sp_grp->addWidget(system_prompt_);
    row->addLayout(sp_grp, 1);

    vl->addLayout(row);

    save_global_btn_ = new QPushButton("保存全局配置");
    save_global_btn_->setFixedHeight(30);
    save_global_btn_->setFixedWidth(180);
    save_global_btn_->setStyleSheet("QPushButton{background:" + QString(ui::colors::BG_RAISED()) + ";color:" +
                                    QString(ui::colors::AMBER()) + ";border:1px solid " + QString(ui::colors::AMBER()) +
                                    ";"
                                    "border-radius:3px;font-weight:600;}"
                                    "QPushButton:hover{background:" +
                                    QString(ui::colors::BG_RAISED()) + ";}");
    connect(save_global_btn_, &QPushButton::clicked, this, &LlmConfigSection::on_save_global);
    vl->addWidget(save_global_btn_);

    return panel;
}

// ============================================================================
// Data Loading
// ============================================================================

void LlmConfigSection::load_providers() {
    provider_list_->blockSignals(true);
    provider_list_->clear();

    auto result = LlmConfigRepository::instance().list_providers();
    if (result.is_ok()) {
        for (const auto& p : result.value()) {
            bool is_fincept = (p.provider.toLower() == "fincept");
            QString display = is_fincept ? "Fincept LLM" : p.provider;
            if (p.is_active)
                display += "  ✓";
            // Show model tag only for non-fincept providers
            if (!is_fincept && !p.model.isEmpty())
                display += "  [" + p.model + "]";
            auto* item = new QListWidgetItem(display);
            item->setData(Qt::UserRole, p.provider);
            // Only the active provider gets amber color
            if (p.is_active)
                item->setForeground(QColor(ui::colors::AMBER()));
            else
                item->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
            provider_list_->addItem(item);
        }
    }

    provider_list_->blockSignals(false);

    // Load global settings
    auto gs = LlmConfigRepository::instance().get_global_settings();
    if (gs.is_ok()) {
        temp_spin_->setValue(gs.value().temperature);
        tokens_spin_->setValue(gs.value().max_tokens);
        system_prompt_->setPlainText(gs.value().system_prompt);
    }

    delete_btn_->setEnabled(false);
    if (provider_list_->count() > 0)
        provider_list_->setCurrentRow(0);
}

void LlmConfigSection::populate_form(const QString& provider) {
    provider_edit_->setText(provider);

    bool is_fincept = (provider.toLower() == "fincept");

    // Populate model combo with fallback suggestions
    model_combo_->blockSignals(true);
    model_combo_->clear();
    model_combo_->addItems(fallback_models(provider));
    model_combo_->blockSignals(false);

    // Auto-fill base URL for known providers
    QString def_url = default_base_url(provider);

    auto result = LlmConfigRepository::instance().list_providers();
    if (result.is_err())
        return;

    for (const auto& p : result.value()) {
        if (p.provider.toLower() == provider.toLower()) {
            // Set model — try to select in combo, or set as editable text
            if (!p.model.isEmpty()) {
                int idx = model_combo_->findText(p.model);
                if (idx >= 0)
                    model_combo_->setCurrentIndex(idx);
                else
                    model_combo_->setCurrentText(p.model);
            }
            base_url_edit_->setText(p.base_url.isEmpty() ? def_url : p.base_url);

            // Tools toggle — always visible
            tools_check_->setChecked(p.tools_enabled);
            tools_check_->setVisible(true);

            if (is_fincept) {
                api_key_edit_->clear();
                auto stored = SettingsRepository::instance().get("fincept_api_key");
                if (stored.is_ok() && !stored.value().isEmpty()) {
                    QString masked = stored.value().left(8) + "...";
                    api_key_edit_->setPlaceholderText("已关联 Fincept 账户: " + masked);
                } else {
                    api_key_edit_->setPlaceholderText("请先登录 Fincept 账户以启用");
                }
                api_key_edit_->setEnabled(false);
                // Fincept is a managed service — hide model/base_url/fetch
                model_combo_->setVisible(false);
                fetch_btn_->setVisible(false);
                base_url_edit_->setVisible(false);
            } else {
                api_key_edit_->setText(p.api_key);
                api_key_edit_->setEnabled(true);
                api_key_edit_->setPlaceholderText("sk-...");
                model_combo_->setVisible(true);
                model_combo_->setEnabled(true);
                fetch_btn_->setVisible(true);
                fetch_btn_->setEnabled(true);
                base_url_edit_->setVisible(true);
                base_url_edit_->setEnabled(true);
            }
            return;
        }
    }

    // New provider — clear form
    api_key_edit_->clear();
    api_key_edit_->setEnabled(!is_fincept);
    if (is_fincept) {
        auto stored = SettingsRepository::instance().get("fincept_api_key");
        if (stored.is_ok() && !stored.value().isEmpty())
            api_key_edit_->setPlaceholderText("已关联 Fincept 账户: " + stored.value().left(8) + "...");
        else
            api_key_edit_->setPlaceholderText("请先登录 Fincept 账户以启用");
        model_combo_->setVisible(false);
        fetch_btn_->setVisible(false);
        base_url_edit_->setVisible(false);
    } else {
        model_combo_->setVisible(true);
        model_combo_->setEnabled(true);
        fetch_btn_->setVisible(true);
        fetch_btn_->setEnabled(true);
        base_url_edit_->setVisible(true);
        base_url_edit_->setEnabled(true);
        base_url_edit_->setText(def_url);
    }
}

// ============================================================================
// Slots
// ============================================================================

void LlmConfigSection::on_provider_selected(int row) {
    if (row < 0 || row >= provider_list_->count()) {
        delete_btn_->setEnabled(false);
        return;
    }

    QString provider = provider_list_->item(row)->data(Qt::UserRole).toString();
    delete_btn_->setEnabled(provider.toLower() != "fincept");
    populate_form(provider);
}

void LlmConfigSection::on_save_provider() {
    QString provider = provider_edit_->text().trimmed().toLower();
    if (provider.isEmpty()) {
        show_status("尚未选择服务商", true);
        return;
    }

    bool is_fincept = (provider == "fincept");

    LlmConfig cfg;
    cfg.provider = provider;
    cfg.api_key = is_fincept ? QString() : api_key_edit_->text().trimmed();
    cfg.model = model_combo_->currentText().trimmed();
    cfg.base_url = base_url_edit_->text().trimmed();
    cfg.is_active = true;
    cfg.tools_enabled = tools_check_->isChecked();

    // Fincept defaults — endpoints are hardcoded in LlmService, base_url not needed
    if (is_fincept) {
        if (cfg.model.isEmpty())
            cfg.model = "MiniMax-M2.7";
        cfg.base_url = {}; // not used for fincept
    }

    // Basic validation
    if (!is_fincept && provider != "ollama" && cfg.api_key.isEmpty()) {
        show_status("API 凭据不能为空: " + provider, true);
        return;
    }
    if (!is_fincept && cfg.model.isEmpty()) {
        show_status("模型名称不能为空", true);
        return;
    }

    // Save first (INSERT OR REPLACE), THEN set active (deactivates others + activates this one)
    auto r2 = LlmConfigRepository::instance().save_provider(cfg);
    if (r2.is_err()) {
        show_status("保存失败: " + QString::fromStdString(r2.error()), true);
        return;
    }
    LlmConfigRepository::instance().set_active(provider);

    show_status("配置已保存并设为当前服务商", false);
    load_providers();
    ai_chat::LlmService::instance().reload_config();
    emit config_changed();

    LOG_INFO(TAG, "LLM provider saved: " + provider + " / " + cfg.model);
}

void LlmConfigSection::on_delete_provider() {
    int row = provider_list_->currentRow();
    if (row < 0)
        return;

    QString provider = provider_list_->item(row)->data(Qt::UserRole).toString();

    if (provider.toLower() == "fincept") {
        show_status("无法移除内置的 Fincept 服务商", true);
        return;
    }

    auto reply = QMessageBox::question(this, "移除服务商", "确定要移除 '" + provider + "' 的配置吗？",
                                       QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    LlmConfigRepository::instance().delete_provider(provider);
    load_providers();
    emit config_changed();
}

void LlmConfigSection::on_save_global() {
    LlmGlobalSettings gs;
    gs.temperature = temp_spin_->value();
    gs.max_tokens = tokens_spin_->value();
    gs.system_prompt = system_prompt_->toPlainText().trimmed();

    auto r = LlmConfigRepository::instance().save_global_settings(gs);
    if (r.is_err()) {
        show_status("全局设置保存失败", true);
        return;
    }

    show_status("全局设置已保存", false);
    emit config_changed();
}

void LlmConfigSection::on_test_connection() {
    QString provider = provider_edit_->text().trimmed().toLower();
    if (provider.isEmpty()) {
        show_status("Select a provider first", true);
        return;
    }

    if (provider == "fincept") {
        // Fincept is a managed service — verify API key exists
        auto stored = SettingsRepository::instance().get("fincept_api_key");
        if (stored.is_ok() && !stored.value().isEmpty())
            show_status("Fincept 已连接 — 凭据有效", false);
        else
            show_status("未连接 — 请先登录 Fincept 账户", true);
        return;
    }

    if (provider != "ollama") {
        if (api_key_edit_->text().trimmed().isEmpty()) {
            show_status("尚未输入 API 凭据，无法测试", true);
            return;
        }
    }

    show_status("正在测试连接...", false);
    test_btn_->setEnabled(false);

    // Real test: fetch models list — if it succeeds, the connection works.
    // on_models_fetched will populate the combo; we just re-enable the button here.
    // Use a one-shot connection for the test status feedback.
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::models_fetched, this,
                    [this, provider, conn](const QString& p, const QStringList& models, const QString& err) {
                        if (p.toLower() != provider.toLower())
                            return;
                        test_btn_->setEnabled(true);
                        if (err.isEmpty())
                            show_status("连接成功 — 已获取到 " + QString::number(models.size()) + " 个可用模型", false);
                        else
                            show_status("连接失败: " + err, true);
                        disconnect(*conn);
                    });

    ai_chat::LlmService::instance().fetch_models(provider, api_key_edit_->text().trimmed(),
                                                 base_url_edit_->text().trimmed());
}

void LlmConfigSection::on_fetch_models() {
    QString provider = provider_edit_->text().trimmed().toLower();
    if (provider.isEmpty()) {
        show_status("Select a provider first", true);
        return;
    }

    if (provider == "fincept") {
        show_status("Fincept 自动管理可用模型", false);
        return;
    }

    if (provider != "ollama") {
        if (api_key_edit_->text().trimmed().isEmpty()) {
            show_status("请先输入凭据，再获取模型列表", true);
            return;
        }
    }

    show_status("正在获取模型列表...", false);
    fetch_btn_->setEnabled(false);

    ai_chat::LlmService::instance().fetch_models(provider, api_key_edit_->text().trimmed(),
                                                 base_url_edit_->text().trimmed());
}

void LlmConfigSection::on_models_fetched(const QString& provider, const QStringList& models, const QString& error) {
    fetch_btn_->setEnabled(true);

    // Only apply if still viewing this provider
    if (provider_edit_->text().trimmed().toLower() != provider.toLower())
        return;

    if (!error.isEmpty()) {
        show_status("获取失败: " + error + " — 已应用建议列表", true);
        return;
    }

    // Save current selection
    QString current = model_combo_->currentText();

    model_combo_->blockSignals(true);
    model_combo_->clear();
    model_combo_->addItems(models);
    model_combo_->blockSignals(false);

    // Restore selection if still valid
    int idx = model_combo_->findText(current);
    if (idx >= 0)
        model_combo_->setCurrentIndex(idx);
    else if (!current.isEmpty())
        model_combo_->setCurrentText(current);
    else if (model_combo_->count() > 0)
        model_combo_->setCurrentIndex(0);

    show_status(provider + " 的 " + QString::number(models.size()) + " 个模型已加载", false);
    LOG_INFO(TAG, QString("Fetched %1 models for %2").arg(models.size()).arg(provider));
}

void LlmConfigSection::show_status(const QString& msg, bool error) {
    status_lbl_->setText(msg);
    status_lbl_->setStyleSheet(error ? "color:" + QString(ui::colors::NEGATIVE()) + ";"
                                     : "color:" + QString(ui::colors::POSITIVE()) + ";");
    status_lbl_->show();
    QTimer::singleShot(4000, status_lbl_, &QLabel::hide);
}

// ============================================================================
// Profiles tab — builder
// ============================================================================

QWidget* LlmConfigSection::build_profiles_tab() {
    auto* w = new QWidget(this);
    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet("QSplitter::handle{background:" + QString(ui::colors::BORDER_DIM()) + ";}");
    splitter->addWidget(build_profile_list_panel());
    splitter->addWidget(build_profile_form_panel());
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({240, 580});
    hl->addWidget(splitter);
    return w;
}

QWidget* LlmConfigSection::build_profile_list_panel() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(240);
    panel->setStyleSheet("background:" + QString(ui::colors::BG_BASE()) + ";border-right:1px solid " +
                         QString(ui::colors::BORDER_DIM()) + ";");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    auto* lbl = new QLabel("方案预设列表");
    lbl->setStyleSheet("color:" + QString(ui::colors::TEXT_SECONDARY()) + ";font-weight:700;letter-spacing:1px;");
    vl->addWidget(lbl);

    auto* hint = new QLabel("预设方案 = 已命名的模型配置，可分配给不同的 Agent 或团队使用。");
    hint->setWordWrap(true);
    hint->setStyleSheet("color:" + QString(ui::colors::TEXT_TERTIARY()) + ";padding-bottom:4px;");
    vl->addWidget(hint);

    profile_list_ = new QListWidget;
    profile_list_->setStyleSheet(
        "QListWidget{background:transparent;border:none;color:" + QString(ui::colors::TEXT_PRIMARY()) +
        ";}"
        "QListWidget::item{padding:8px 6px;border-radius:3px;}"
        "QListWidget::item:selected{background:" +
        QString(ui::colors::BG_RAISED()) + ";color:" + QString(ui::colors::AMBER()) +
        ";}"
        "QListWidget::item:hover{background:" +
        QString(ui::colors::BG_RAISED()) + ";}");
    connect(profile_list_, &QListWidget::currentRowChanged, this, &LlmConfigSection::on_profile_selected);
    vl->addWidget(profile_list_, 1);

    auto* btn_row = new QHBoxLayout;
    auto* add_btn = new QPushButton("+ 新建预设");
    add_btn->setStyleSheet("QPushButton{background:" + QString(ui::colors::BG_RAISED()) + ";color:" +
                           QString(ui::colors::AMBER()) + ";border:1px solid " + QString(ui::colors::AMBER()) +
                           ";"
                           "border-radius:3px;padding:5px 10px;font-weight:600;}"
                           "QPushButton:hover{background:" +
                           QString(ui::colors::BG_RAISED()) + ";}");
    connect(add_btn, &QPushButton::clicked, this, [this]() {
        editing_profile_id_.clear();
        clear_profile_form();
    });
    btn_row->addWidget(add_btn);

    profile_delete_btn_ = new QPushButton("删除");
    profile_delete_btn_->setEnabled(false);
    profile_delete_btn_->setStyleSheet("QPushButton{background:transparent;color:" + QString(ui::colors::NEGATIVE()) +
                                       ";border:1px solid " + QString(ui::colors::NEGATIVE()) +
                                       ";"
                                       "border-radius:3px;padding:5px 10px;}"
                                       "QPushButton:hover{background:" +
                                       QString(ui::colors::BG_RAISED()) +
                                       ";}"
                                       "QPushButton:disabled{color:" +
                                       QString(ui::colors::BORDER_BRIGHT()) +
                                       ";border-color:" + QString(ui::colors::BORDER_BRIGHT()) + ";}");
    connect(profile_delete_btn_, &QPushButton::clicked, this, &LlmConfigSection::on_delete_profile);
    btn_row->addWidget(profile_delete_btn_);
    vl->addLayout(btn_row);

    return panel;
}

QWidget* LlmConfigSection::build_profile_form_panel() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:" + QString(ui::colors::BG_BASE()) +
                          ";}"
                          "QScrollBar:vertical{width:6px;background:" +
                          QString(ui::colors::BG_SURFACE()) +
                          ";}"
                          "QScrollBar::handle:vertical{background:" +
                          QString(ui::colors::BORDER_BRIGHT()) + ";}");

    auto* panel = new QWidget(this);
    panel->setStyleSheet("background:" + QString(ui::colors::BG_BASE()) + ";");
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(20, 16, 20, 16);
    vl->setSpacing(10);

    static auto lbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet("color:" + QString(ui::colors::TEXT_SECONDARY()) + ";font-weight:700;letter-spacing:1px;");
        return l;
    };
    static auto field_style = []() {
        return QString("background:" + QString(ui::colors::BG_RAISED()) +
                       ";color:" + QString(ui::colors::TEXT_PRIMARY()) + ";border:1px solid " +
                       QString(ui::colors::BORDER_MED()) + ";padding:6px 10px;");
    };

    vl->addWidget(lbl("方案名称"));
    profile_name_edit_ = new QLineEdit;
    profile_name_edit_->setPlaceholderText("例如: 快速 Groq, 严谨 Claude, 深度思考");
    profile_name_edit_->setStyleSheet(field_style());
    vl->addWidget(profile_name_edit_);

    vl->addWidget(lbl("所属服务商"));
    profile_provider_combo_ = new QComboBox;
    profile_provider_combo_->addItems(KNOWN_PROVIDERS);
    profile_provider_combo_->setStyleSheet(
        QString("QComboBox{%1}QComboBox::drop-down{border:none;}").arg(field_style()));
    connect(profile_provider_combo_, &QComboBox::currentTextChanged, this,
            &LlmConfigSection::on_profile_provider_changed);
    vl->addWidget(profile_provider_combo_);

    vl->addWidget(lbl("具体模型 (Model)"));
    profile_model_combo_ = new QComboBox;
    profile_model_combo_->setEditable(true);
    profile_model_combo_->setStyleSheet(QString("QComboBox{%1}QComboBox::drop-down{border:none;}").arg(field_style()));
    vl->addWidget(profile_model_combo_);

    vl->addWidget(lbl("独立 API 凭据 (API Key)"));
    profile_api_key_edit_ = new QLineEdit;
    profile_api_key_edit_->setEchoMode(QLineEdit::Password);
    profile_api_key_edit_->setPlaceholderText("留空则继承服务商默认配置");
    profile_api_key_edit_->setStyleSheet(field_style());
    vl->addWidget(profile_api_key_edit_);

    vl->addWidget(lbl("独立代理地址 (Base URL)"));
    profile_base_url_edit_ = new QLineEdit;
    profile_base_url_edit_->setPlaceholderText("留空则使用服务商默认地址");
    profile_base_url_edit_->setStyleSheet(field_style());
    vl->addWidget(profile_base_url_edit_);

    auto* param_row = new QHBoxLayout;
    auto* temp_col = new QVBoxLayout;
    temp_col->addWidget(lbl("生成温度 (Temp)"));
    profile_temp_spin_ = new QDoubleSpinBox;
    profile_temp_spin_->setRange(0.0, 2.0);
    profile_temp_spin_->setSingleStep(0.1);
    profile_temp_spin_->setValue(0.7);
    profile_temp_spin_->setStyleSheet(field_style());
    temp_col->addWidget(profile_temp_spin_);
    param_row->addLayout(temp_col);

    auto* tok_col = new QVBoxLayout;
    tok_col->addWidget(lbl("最大 Token"));
    profile_tokens_spin_ = new QSpinBox;
    profile_tokens_spin_->setRange(256, 128000);
    profile_tokens_spin_->setSingleStep(256);
    profile_tokens_spin_->setValue(4096);
    profile_tokens_spin_->setStyleSheet(field_style());
    tok_col->addWidget(profile_tokens_spin_);
    param_row->addLayout(tok_col);
    vl->addLayout(param_row);

    vl->addWidget(lbl("自定义系统提示词 (可选)"));
    profile_prompt_edit_ = new QPlainTextEdit;
    profile_prompt_edit_->setPlaceholderText("留空则使用全局系统提示词");
    profile_prompt_edit_->setMaximumHeight(80);
    profile_prompt_edit_->setStyleSheet(QString("QPlainTextEdit{%1}").arg(field_style()));
    vl->addWidget(profile_prompt_edit_);

    auto* btn_row = new QHBoxLayout;
    profile_save_btn_ = new QPushButton("保存方案预设");
    profile_save_btn_->setStyleSheet("QPushButton{background:" + QString(ui::colors::AMBER()) +
                                     ";color:" + QString(ui::colors::BG_BASE()) +
                                     ";border:none;padding:8px 20px;"
                                     "font-weight:700;letter-spacing:1px;}"
                                     "QPushButton:hover{background:" +
                                     QString(ui::colors::AMBER_DIM()) + ";}");
    connect(profile_save_btn_, &QPushButton::clicked, this, &LlmConfigSection::on_save_profile);
    btn_row->addWidget(profile_save_btn_);

    profile_default_btn_ = new QPushButton("设为系统默认");
    profile_default_btn_->setEnabled(false);
    profile_default_btn_->setStyleSheet("QPushButton{background:transparent;color:" + QString(ui::colors::AMBER()) +
                                        ";border:1px solid " + QString(ui::colors::AMBER()) +
                                        ";"
                                        "padding:8px 20px;font-weight:600;}"
                                        "QPushButton:hover{background:" +
                                        QString(ui::colors::BG_RAISED()) +
                                        ";}"
                                        "QPushButton:disabled{color:" +
                                        QString(ui::colors::BORDER_BRIGHT()) +
                                        ";border-color:" + QString(ui::colors::BORDER_BRIGHT()) + ";}");
    connect(profile_default_btn_, &QPushButton::clicked, this, &LlmConfigSection::on_set_default_profile);
    btn_row->addWidget(profile_default_btn_);
    btn_row->addStretch();
    vl->addLayout(btn_row);

    profile_status_lbl_ = new QLabel;
    profile_status_lbl_->hide();
    vl->addWidget(profile_status_lbl_);

    vl->addStretch();
    scroll->setWidget(panel);
    return scroll;
}

// ============================================================================
// Profiles tab — data helpers
// ============================================================================

void LlmConfigSection::load_profiles() {
    profile_list_->blockSignals(true);
    profile_list_->clear();

    auto result = LlmProfileRepository::instance().list_profiles();
    if (result.is_ok()) {
        for (const auto& p : result.value()) {
            QString display = p.name;
            if (p.is_default)
                display += "  ★";
            display += QString("  [%1 / %2]").arg(p.provider, p.model_id);
            auto* item = new QListWidgetItem(display);
            item->setData(Qt::UserRole, p.id);
            if (p.is_default)
                item->setForeground(QColor("" + QString(ui::colors::AMBER()) + ""));
            profile_list_->addItem(item);
        }
    }

    profile_list_->blockSignals(false);
}

void LlmConfigSection::populate_profile_form(const LlmProfile& p) {
    profile_name_edit_->setText(p.name);

    int idx = profile_provider_combo_->findText(p.provider, Qt::MatchFixedString | Qt::MatchCaseSensitive);
    if (idx >= 0)
        profile_provider_combo_->setCurrentIndex(idx);
    else {
        profile_provider_combo_->addItem(p.provider);
        profile_provider_combo_->setCurrentText(p.provider);
    }

    // Populate model combo from fallback list, then select saved model
    profile_model_combo_->clear();
    profile_model_combo_->addItems(fallback_models(p.provider));
    profile_model_combo_->setCurrentText(p.model_id);

    profile_api_key_edit_->setText(p.api_key);
    profile_base_url_edit_->setText(p.base_url);
    profile_temp_spin_->setValue(p.temperature);
    profile_tokens_spin_->setValue(p.max_tokens);
    profile_prompt_edit_->setPlainText(p.system_prompt);

    editing_profile_id_ = p.id;
    profile_delete_btn_->setEnabled(true);
    profile_default_btn_->setEnabled(!p.is_default);
}

void LlmConfigSection::clear_profile_form() {
    profile_name_edit_->clear();
    profile_provider_combo_->setCurrentIndex(0);
    on_profile_provider_changed(profile_provider_combo_->currentText());
    profile_api_key_edit_->clear();
    profile_base_url_edit_->clear();
    profile_temp_spin_->setValue(0.7);
    profile_tokens_spin_->setValue(4096);
    profile_prompt_edit_->clear();
    profile_list_->clearSelection();
    profile_delete_btn_->setEnabled(false);
    profile_default_btn_->setEnabled(false);
    editing_profile_id_.clear();
}

void LlmConfigSection::show_profile_status(const QString& msg, bool error) {
    profile_status_lbl_->setText(msg);
    profile_status_lbl_->setStyleSheet(error ? "color:" + QString(ui::colors::NEGATIVE()) + ";"
                                             : "color:" + QString(ui::colors::POSITIVE()) + ";");
    profile_status_lbl_->show();
    QTimer::singleShot(4000, profile_status_lbl_, &QLabel::hide);
}

// ============================================================================
// Profiles tab — slots
// ============================================================================

void LlmConfigSection::on_profile_selected(int row) {
    if (row < 0)
        return;
    auto* item = profile_list_->item(row);
    if (!item)
        return;
    QString id = item->data(Qt::UserRole).toString();
    auto r = LlmProfileRepository::instance().get_profile(id);
    if (r.is_ok())
        populate_profile_form(r.value());
}

void LlmConfigSection::on_profile_provider_changed(const QString& provider) {
    profile_model_combo_->clear();
    profile_model_combo_->addItems(fallback_models(provider));
    profile_base_url_edit_->setText(default_base_url(provider));

    // Pre-fill api_key from saved provider if present and field is empty
    if (profile_api_key_edit_->text().isEmpty()) {
        auto providers = LlmConfigRepository::instance().list_providers();
        if (providers.is_ok()) {
            for (const auto& p : providers.value()) {
                if (p.provider.toLower() == provider.toLower() && !p.api_key.isEmpty()) {
                    profile_api_key_edit_->setText(p.api_key);
                    break;
                }
            }
        }
    }
}

void LlmConfigSection::on_save_profile() {
    QString name = profile_name_edit_->text().trimmed();
    if (name.isEmpty()) {
        show_profile_status("方案名称不能为空", true);
        return;
    }
    QString provider = profile_provider_combo_->currentText().trimmed();
    QString model = profile_model_combo_->currentText().trimmed();
    if (model.isEmpty()) {
        show_profile_status("必须要指定模型(Model)", true);
        return;
    }

    // If api_key is empty, inherit from saved provider
    QString api_key = profile_api_key_edit_->text().trimmed();
    if (api_key.isEmpty()) {
        auto providers = LlmConfigRepository::instance().list_providers();
        if (providers.is_ok()) {
            for (const auto& p : providers.value()) {
                if (p.provider.toLower() == provider.toLower()) {
                    api_key = p.api_key;
                    break;
                }
            }
        }
    }

    LlmProfile profile;
    profile.id = editing_profile_id_; // empty = new (repo generates UUID)
    profile.name = name;
    profile.provider = provider.toLower();
    profile.model_id = model;
    profile.api_key = api_key;
    profile.base_url = profile_base_url_edit_->text().trimmed();
    profile.temperature = profile_temp_spin_->value();
    profile.max_tokens = profile_tokens_spin_->value();
    profile.system_prompt = profile_prompt_edit_->toPlainText().trimmed();
    profile.is_default = false;

    auto r = LlmProfileRepository::instance().save_profile(profile);
    if (r.is_err()) {
        show_profile_status("保存失败: " + QString::fromStdString(r.error()), true);
        return;
    }

    show_profile_status("方案预设已保存", false);
    load_profiles();
    emit config_changed();
    LOG_INFO(TAG, QString("LLM profile saved: %1 (%2 / %3)").arg(name, provider, model));
}

void LlmConfigSection::on_delete_profile() {
    if (editing_profile_id_.isEmpty())
        return;
    auto r = LlmProfileRepository::instance().delete_profile(editing_profile_id_);
    if (r.is_ok()) {
        clear_profile_form();
        load_profiles();
        emit config_changed();
        LOG_INFO(TAG, "LLM profile deleted: " + editing_profile_id_);
    } else {
        show_profile_status("删除失败: " + QString::fromStdString(r.error()), true);
    }
}

void LlmConfigSection::on_set_default_profile() {
    if (editing_profile_id_.isEmpty())
        return;
    auto r = LlmProfileRepository::instance().set_default(editing_profile_id_);
    if (r.is_ok()) {
        profile_default_btn_->setEnabled(false);
        load_profiles();
        emit config_changed();
        LOG_INFO(TAG, "LLM default profile set: " + editing_profile_id_);
    } else {
        show_profile_status("设置失败: " + QString::fromStdString(r.error()), true);
    }
}

} // namespace fincept::screens
