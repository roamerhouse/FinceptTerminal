#pragma once

#include <QString>
#include <QMap>

namespace fincept::ui {

class Localization {
public:
    static QString translate_agent_name(const QString& name) {
        static const QMap<QString, QString> agents = {
            {"Core Agent", "核心智能体"},
            {"Charlie Munger Mental Models Agent", "查理·芒格思维模型智能体"},
            {"Charlie Munger Agent", "查理·芒格智能体"},
            {"Ray Dalio Agent", "瑞·达利欧智能体"},
            {"Warren Buffett Agent", "巴菲特智能体"},
            {"George Soros Agent", "索罗斯智能体"},
            {"System Agent", "系统智能体"}
        };
        return agents.value(name, name);
    }

    static QString translate_category(const QString& cat) {
        QString c = cat.toLower();
        static const QMap<QString, QString> categories = {
            {"general", "通用"},
            {"trading", "交易"},
            {"finance", "金融"},
            {"investment", "投资"},
            {"portfolio", "组合管理"},
            {"market", "市场数据"},
            {"sec_edgar", "证监会查询"},
            {"search", "搜索"},
            {"web", "网页浏览"},
            {"development", "开发工具"},
            {"devops", "运维服务"},
            {"database", "数据库"},
            {"data", "数据处理"},
            {"communication", "通讯工具"},
            {"google", "谷歌服务"},
            {"aws", "亚马逊云 (AWS)"},
            {"ai", "人工智能"},
            {"media", "媒体/图像"},
            {"research", "研报分析"},
            {"utilities", "常用工具"},
            {"business", "商务办公"},
            {"social", "社交媒体"},
            {"scraping", "数据采集"},
            {"memory", "短期记忆"},
            {"workflow", "工作流控制"},
            {"blockchain", "区块链/Web3"},
            {"file_generation", "文件生成"},
            {"internal-mcp", "内置核心 (MCP)"},
            {"hedgefundagents", "对冲基金策略"},
            {"geopoliticsagents", "地缘政治分析"},
            {"economicagents", "宏观经济研究"},
            {"traderinvestoragents", "交易者/投资者"}
        };
        return categories.value(c, cat); // Return original if not found (keep it uppercase if was uppercase)
    }

    static QString translate_team_mode(const QString& mode) {
        static const QMap<QString, QString> modes = {
            {"coordinate", "领导者协调 (Coordinate)"},
            {"route", "意图路由 (Route)"},
            {"collaborate", "全员协作 (Collaborate)"}
        };
        return modes.value(mode, mode);
    }
};

} // namespace fincept::ui
