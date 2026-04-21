import sqlite3
import json
import os

# 数据库路径
db_path = r'C:\Users\19547\AppData\Local\com.fincept.terminal\data\fincept.db'
# 智能体定义根目录
agents_root = r'e:\git tools\FinceptTerminal\fincept-qt\scripts\agents'

def sync():
    if not os.path.exists(db_path):
        print(f"Error: Database not found at {db_path}")
        return

    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    count_updated = 0
    count_inserted = 0

    # 递归搜索所有的 agent_definitions.json
    for root, dirs, files in os.walk(agents_root):
        if 'agents.deleted' in root: # 跳过备份目录
            continue
            
        if 'agent_definitions.json' in files:
            json_path = os.path.join(root, 'agent_definitions.json')
            print(f"Processing: {json_path}")
            
            try:
                with open(json_path, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                    agents = data.get('agents', [])
                    
                    for agent in agents:
                        a_id = agent['id']
                        a_name = agent['name']
                        a_desc = agent.get('description', '')
                        a_cat = agent.get('category', 'general')
                        # 序列化配置
                        a_config_json = json.dumps(agent.get('config', {}), ensure_ascii=False)
                        
                        # 检查是否存在
                        cursor.execute("SELECT id FROM agent_configs WHERE id=?", (a_id,))
                        if cursor.fetchone():
                            # 更新
                            cursor.execute("""
                                UPDATE agent_configs 
                                SET name=?, description=?, config_json=?, category=?, updated_at=datetime('now')
                                WHERE id=?
                            """, (a_name, a_desc, a_config_json, a_cat, a_id))
                            count_updated += 1
                        else:
                            # 插入
                            cursor.execute("""
                                INSERT INTO agent_configs (id, name, description, config_json, category, is_default, is_active)
                                VALUES (?, ?, ?, ?, ?, 1, 1)
                            """, (a_id, a_name, a_desc, a_config_json, a_cat))
                            count_inserted += 1
            except Exception as e:
                print(f"Error processing {json_path}: {e}")

    conn.commit()
    conn.close()
    print(f"\nSync Complete!")
    print(f"Updated: {count_updated}")
    print(f"Inserted: {count_inserted}")
    print(f"Total in Database: {count_updated + count_inserted}")

if __name__ == "__main__":
    sync()
