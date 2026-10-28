"""Generate firmware mapping from the dashboard and read-only furnace monitor configuration."""
import json
from pathlib import Path
import sys
source, target = map(Path, sys.argv[1:])
data = json.loads(source.read_text(encoding="utf-8"))
expected = ['deck', 'breezeway', 'blinds', 'washer1', 'washer2', 'dryer1', 'dryer2', 'kids_heat', 'kids_temp', 'vacuum', 'camera']
if list(data['entities']) != expected:
    raise ValueError('Unexpected HA entity keys/order')
lines = ['/* Generated entity mapping, no credentials. */', 'static const ha_spec_t ha_specs[HA_COUNT] = {']
monitors = data.get('furnace_monitor', {})
if list(monitors) != ['furnace_current', 'furnace_power']:
    raise ValueError('Unexpected furnace monitor keys/order')
for key, item in {**data['entities'], **monitors}.items():
    domain, name = item['entity_id'].split('.', 1)
    required = {'light':'light', 'switch':'input_boolean', 'cover':'cover', 'laundry':'sensor', 'temperature':'sensor', 'vacuum':'vacuum', 'camera':'camera', 'current':'sensor', 'power':'sensor'}
    if domain != required[item['kind']] or not all(c in 'abcdefghijklmnopqrstuvwxyz0123456789_' for c in name):
        raise ValueError('Invalid entity mapping')
    lines.append('    {' + ','.join(json.dumps(s, ensure_ascii=True) for s in [key, item['entity_id'], item['label'], item['kind'], domain]) + '},')
lines.append('};')
target.parent.mkdir(parents=True, exist_ok=True)
text = '\n'.join(lines) + '\n'
if not target.exists() or target.read_text() != text:
    target.write_text(text, encoding='utf-8')
