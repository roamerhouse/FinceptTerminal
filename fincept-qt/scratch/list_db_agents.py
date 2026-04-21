import sqlite3
import os

# 尝试所有可能的路径
paths = [
    os.path.join(os.environ.get('LOCALAPPDATA', ''), 'com.fincept.terminal', 'data', 'fincept.db'),
    r'e:\git tools\FinceptTerminal\fincept-qt\data\fincept.db',
    r'e:\git tools\FinceptTerminal\fincept.db'
]

for db_path in paths:
    if os.path.exists(db_path):
        print(f"Found database at: {db_path}")
        try:
            conn = sqlite3.connect(db_path)
            cursor = conn.cursor()
            cursor.execute("SELECT id, name, category FROM agent_configs")
            agents = cursor.fetchall()
            print(f"Total Agents: {len(agents)}")
            for agent in agents:
                print(f"ID: {agent[0]} | Name: {agent[1]} | Category: {agent[2]}")
            conn.close()
            break
        except Exception as e:
            print(f"Error accessing DB: {e}")
else:
    print("Database file not found.")
