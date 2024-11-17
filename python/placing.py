#! /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3

import pcbnew

rlist = ['R59', 'R119', 'R118', 'R121', 'R120', 'R60', 'R63']

board = pcbnew.LoadBoard('../CMPE2750.kicad_pcb')

u7 = board.FindFootprintByReference('U7')

print(u7.GetPosition())

template = []

for ref in rlist:
	r = board.FindFootprintByReference(ref)
	print(ref, r.GetPosition() - u7.GetPosition(), r.GetOrientation())
	for pad in r.Pads():
		net = pad.GetNet()
		print(net.GetNetCode())
	template.append((r.GetPosition() - u7.GetPosition(), r.GetOrientation()))


def find_pulldown(net, board):
	gnd = board.GetNetcodeFromNetname('GND')
	nc = net.GetNetCode()
	for f in board.GetFootprints():
		pads = list(f.Pads())
		if len(pads) == 2:
			pc0 = pads[0].GetNetCode()
			if pc0 == gnd:
				if pads[1].GetNetCode() == nc:
					return f
				else:
					continue
			elif pc0 == nc:
				if pads[1].GetNetCode() == gnd:
					return f


def get_resistors(fp, board):
	d = {}
	for pad in fp.Pads():
		net = pad.GetNet()
		if net.GetShortNetname().startswith('p'):
			print(net.GetShortNetname(), net.GetNetCode())
			d[pad.GetNumber()] = find_pulldown(net, board)
	return d

def get_pulldown_template(fp, board):
	pulldowns = get_resistors(fp, board)
	pos = fp.GetPosition()
	return { pin: (r.GetPosition() - pos, r.GetOrientation(), r.GetLayer()) for pin, r in pulldowns.items() }

def apply_pulldown_template(fp, board, template):
	pos = fp.GetPosition()
	for pin in template:
		pad = fp.FindPadByNumber(pin)
		print(pad, pin)
		r = find_pulldown(pad.GetNet(), board)
		print(r.GetReference())
		print(r.GetPosition())
		r.SetPosition(pos + template[pin][0])
		print(r.GetPosition())
		r.SetLayerAndFlip(template[pin][2])
		r.SetOrientation(template[pin][1])

print(get_resistors(u7, board))
template = get_pulldown_template(u7, board)
print(template)


for i in [8, 9, 12]:
	print('U' + str(i))
	apply_pulldown_template(board.FindFootprintByReference('U' + str(i)), board, template)

#board.Save('../CMPE2750_mod.kicad_pcb')

def find_net_components(net, board):
	if isinstance(net, pcbnew.NETINFO_ITEM):
		net = net.GetNetCode()
	elif isinstance(net, str):
		net = board.GetNetcodeFromNetname(net)
	return (fp for fp in board.GetFootprints() if any(pad.GetNetCode() == net for pad in fp.Pads()))

def get_template_from_pins(fp, board, pins):
	d = {}
	for pin in pins:
		net = fp.FindPadByNumber(pin).GetNetCode()
		netcomp = { c.GetValue(): (c.GetPosition() - fp.GetPosition(), c.GetOrientation(), c.GetLayer()) for c in find_net_components(net, board) if c != fp }
		d[pin] = netcomp
	return d

def apply_pin_template(fp, board, template):
	for pin in template:
		print(pin)
		net = fp.FindPadByNumber(pin).GetNetCode()
		net_template = template[pin]
		for comp in find_net_components(net, board):
			if comp != fp:
				print(comp.GetReference())
				offset, orientation, layer = net_template[comp.GetValue()]
				comp.SetPosition(fp.GetPosition() + offset)
				comp.SetLayerAndFlip(layer)
				comp.SetOrientation(orientation)


q10 = board.FindFootprintByReference('Q10')
template = get_template_from_pins(q10, board, [1, 3])
print(template)

orientation, layer = q10.GetOrientation(), q10.GetLayer()


for i in range(11, 19):
	print(f'Q{i}')
	q = board.FindFootprintByReference(f'Q{i}')
	q.SetLayerAndFlip(layer)
	q.SetOrientation(orientation)
	apply_pin_template(q, board, template)


for i in range(28, 37):
	print(f'Q{i}')
	q = board.FindFootprintByReference(f'Q{i}')
	q.SetLayerAndFlip(layer)
	q.SetOrientation(orientation)
	apply_pin_template(q, board, template)

#board.Save('../CMPE2750_mod.kicad_pcb')

#step = 6500000


q38 = board.FindFootprintByReference('Q38')
template = get_template_from_pins(q38, board, [1, 3])
print(template)


for i in range(37, 51):
	print(f'Q{i}')
	q = board.FindFootprintByReference(f'Q{i}')
	apply_pin_template(q, board, template)

board.Save('../CMPE2750_mod.kicad_pcb')
