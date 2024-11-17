
import pcbnew
import itertools


bd = pcbnew.LoadBoard('../CMPE2750.kicad_pcb')

s2 = bd.FindFootprintByReference('SW2')

pp16 = s2.FindPadByNumber(16).GetPosition()

diff = pcbnew.VECTOR2I_MM(2.54, 0)

position = pp16 - pcbnew.VECTOR2I_MM(0, 5)

def place(itr, pre):
	global bd, diff, position
	for i in itr:
		d = bd.FindFootprintByReference(pre + str(i))
		d.SetPosition(position)
		d.SetOrientation(pcbnew.EDA_ANGLE(90))
		position += diff

place(range(3,7), 'D')
place([10, 8, 7, 9], 'D')
position += diff
place([14, 12, 11, 13], 'D')
place(range(15, 19), 'D')

pp9 = s2.FindPadByNumber(9).GetPosition()
s3 = bd.FindFootprintByReference('SW3')
pp16 = s3.FindPadByNumber(16).GetPosition()
s3.SetPosition(pp9 + pcbnew.VECTOR2I_MM(5.08, 0) - pp16 + s3.GetPosition())

#q51 = bd.FindFootprintByReference('Q51')
#court = q51.GetBoundingBox(False, False)
#diff = pcbnew.VECTOR2I_MM(pcbnew.ToMM(court.GetWidth()) + 1, 0)
#position = q51.GetPosition() + diff
#orientation = q51.GetOrientation()
#
#place(range(52, 55), 'Q')
#place([58, 56, 55, 57], 'Q')
#place([62, 60, 59, 61], 'Q')
#place(range(63, 67), 'Q')
#
#for i in range(52, 67):
#	bd.FindFootprintByReference(f'Q{i}').SetOrientation(orientation)

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


r144 = bd.FindFootprintByReference('R144')
position = r144.GetPosition() - bd.FindFootprintByReference('SW2').FindPadByNumber(1).GetPosition()
layer = r144.GetLayer()
orientation = r144.GetOrientation()

for pad in itertools.chain(bd.FindFootprintByReference('SW2').Pads(), bd.FindFootprintByReference('SW3').Pads()):
	net = pad.GetNet()
	if net.GetShortNetname().startswith('b'):
		r = find_pulldown(net, bd)
		r.SetPosition(position + pad.GetPosition())
		r.SetLayerAndFlip(layer)
		r.SetOrientation(orientation)

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

d3 = bd.FindFootprintByReference('D3')
template = get_template_from_pins(d3, bd, [2])
print(template)


for i in range(4, 19):
	print(f'D{i}')
	d = bd.FindFootprintByReference(f'D{i}')
	apply_pin_template(d, bd, template)



bd.Save('../CMPE2750_mod.kicad_pcb')

