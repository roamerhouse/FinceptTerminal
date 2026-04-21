import json
import sqlite3
import os
import glob

# 数据库路径
DB_PATH = r"C:\Users\19547\AppData\Local\com.fincept.terminal\data\fincept.db"
# 配置文件基础路径
BASE_DIR = r"E:\git tools\FinceptTerminal\fincept-qt\scripts\agents"

def sync_agents():
    if not os.path.exists(DB_PATH):
        print(f"Error: Database not found at {DB_PATH}")
        return

    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()

    count = 0

    # 1. 处理成组定义的 JSON 文件
    group_files = [
        "EconomicAgents/configs/agent_definitions.json",
        "GeopoliticsAgents/configs/agent_definitions.json",
        "TraderInvestorsAgent/configs/agent_definitions.json"
    ]

    for rel_path in group_files:
        full_path = os.path.join(BASE_DIR, rel_path)
        if not os.path.exists(full_path):
            print(f"Warning: File not found: {full_path}")
            continue

        with open(full_path, 'r', encoding='utf-8') as f:
            agents = json.load(f)
            for agent in agents:
                agent_id = agent.get('id')
                name = agent.get('name')
                description = agent.get('description')
                # config 字段在数据库中是 JSON 字符串
                config_str = json.dumps(agent.get('config', {}), ensure_ascii=False)
                
                cursor.execute("""
                    UPDATE agent_configs 
                    SET name = ?, description = ?, config = ? 
                    WHERE id = ?
                """, (name, description, config_str, agent_id))
                
                if cursor.rowcount > 0:
                    print(f"Synced group agent: {agent_id} ({name})")
                    count += 1
                else:
                    # 如果不存在则尝试插入，保留原始分类等信息
                    category = agent.get('category', '')
                    cursor.execute("""
                        INSERT INTO agent_configs (id, name, description, config, category)
                        VALUES (?, ?, ?, ?, ?)
                    """, (agent_id, name, description, config_str, category))
                    print(f"Inserted new agent: {agent_id} ({name})")
                    count += 1

    # 2. 处理单体定义的 JSON 文件 (finagent_core)
    core_dir = os.path.join(BASE_DIR, "finagent_core/configs")
    if os.path.exists(core_dir):
        for json_file in glob.glob(os.path.join(core_dir, "*.json")):
            with open(json_file, 'r', encoding='utf-8') as f:
                agent = json.load(f)
                agent_id = agent.get('id')
                name = agent.get('name')
                description = agent.get('description')
                config_str = json.dumps(agent.get('config', {}), ensure_ascii=False)
                
                cursor.execute("""
                    UPDATE agent_configs 
                    SET name = ?, description = ?, config = ? 
                    WHERE id = ?
                """, (name, description, config_str, agent_id))
                
                if cursor.rowcount > 0:
                    print(f"Synced core agent: {agent_id} ({name})")
                    count += 1
                else:
                    category = agent.get('category', 'finagent_core')
                    cursor.execute("""
                        INSERT INTO agent_configs (id, name, description, config, category)
                        VALUES (?, ?, ?, ?, ?)
                    """, (agent_id, name, description, config_str, category))
                    print(f"Inserted new core agent: {agent_id} ({name})")
                    count += 1

    conn.commit()
    conn.close()
    print(f"\nSuccessfully synchronized {count} agents to database.")

if __name__ == "__main__":
    sync_agents()
