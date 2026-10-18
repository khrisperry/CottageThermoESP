# Hardware / printed part development

Start with \`cap_fit_kit.scad\` for six **unverified round-bore fitting coupons** with nominal IDs **18, 19.5, 21, 22.5, 24, 25 mm**. This is NOT a final friction cap, not a servo coupling, and not intended for attachment to an operating gas furnace.

## OpenSCAD export

\`\`\`powershell
openscad -o cap_fit_kit.stl .\hardware\cap_fit_kit.scad
\`\`\`

Alternatively open in OpenSCAD and use F6 Render → Export STL. Use \`item=0\` through \`item=5\` in file to print a single variant, or \`-1\` to print the full set. Orient the flat closed base on the print bed with the cup opening facing up.

The approximate outer diameter equals nominal inner diameter + 4.4 mm. Check actual printed bore using calipers; shrinkage and printer flow can cause material-dependent variance. Labels indicate **nominal bore**, not measured real diameter.

Do not make a tighter fit by forcing cups onto furnace controls. Wait for caliper measurements and an approved mounting design.

Next CAD after actual measurements:
- precise contoured knob cap with a hand grip;
- T20/T25 verified-hub compatible keyed release coupling;
- stationary secure servo holder/bracket with clearance to timer, vent and panel;
- check actual distance along rotational axis and allow complete manual removal;
- resolve absolute magnetic encoder placement.
