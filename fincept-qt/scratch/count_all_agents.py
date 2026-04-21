import json
import os

base_dir = r'e:\git tools\FinceptTerminal\fincept-qt\scripts\agents'
all_agents = []

for root, dirs, files in os.walk(base_dir):
    if 'agent_definitions.json' in files:
        path = os.path.join(root, 'agent_definitions.json')
        try:
            with open(path, 'r', encoding='utf-8') as f:
                content = json.load(f)
                if 'agents' in content:
                    all_agents.extend(content['agents'])
        except Exception as e:
            print(f"Error reading {path}: {e}")

print(f"Total Agents found in JSONs: {len(all_agents)}")
for agent in all_agents:
    print(f"ID: {agent['id']} | Category: {agent.get('category', 'N/A')}")
