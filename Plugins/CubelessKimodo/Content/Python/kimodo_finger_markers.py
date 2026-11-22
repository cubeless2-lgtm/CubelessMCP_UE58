"""Finger marker angles are bind-relative flexion in degrees, ordered base to tip."""
import math

DIGITS=("thumb","index","middle","ring","pinky")
PRESETS={
    "open":((8,5,4),(2,4,4),(4,6,5),(6,9,7),(8,11,9)),
    "chair_grip":((28,25,32),(42,55,38),(49,64,42),(53,69,46),(54,68,48)),
    "doorknob_grip":((36,31,36),(35,47,30),(45,58,36),(50,62,42),(52,62,44)),
}

def validate(items,frames):
    if not isinstance(items,list):raise ValueError("finger_markers must be an array")
    used=set();ids=set()
    for m in items:
        start,end=m["start_frame"],m["end_frame"]
        side=m["side"];preset=m.get("pose");overrides=m.get("curl_deg",{})
        if (not isinstance(m.get("id"),str) or not m["id"] or m["id"] in ids or side not in ("l","r")
            or type(start)!=int or type(end)!=int or not 0<=start<=end<frames
            or preset is not None and preset not in PRESETS or not isinstance(overrides,dict)
            or not preset and not overrides):raise ValueError(f"Invalid finger marker: {m}")
        ids.add(m["id"])
        for digit,v in overrides.items():
            if digit not in DIGITS or not isinstance(v,list) or len(v)!=3 or not all(type(a) in (int,float) and math.isfinite(a) for a in v):
                raise ValueError(f"Invalid curl_deg: {digit}")
        for key in ("blend_in_frames","blend_out_frames"):
            if type(m.get(key,0))!=int or m.get(key,0)<0:raise ValueError(key)
        if m.get("blend_in_frames",0)+m.get("blend_out_frames",0)>end-start or not 0<=m.get("weight",1)<=1:raise ValueError("Finger marker envelope")
        span={(side,d,f) for d in (DIGITS if preset else overrides) for f in range(start,end+1)}
        if used & span:raise ValueError("Overlapping finger markers")
        used.update(span)
    return items

def targets(items,frame):
    result={}
    for m in items:
        a,b=m["start_frame"],m["end_frame"]
        if not a<=frame<=b:continue
        t=min(1,(frame-a)/m["blend_in_frames"] if m.get("blend_in_frames") else 1,
                (b-frame)/m["blend_out_frames"] if m.get("blend_out_frames") else 1)
        weight=t*t*(3-2*t)*m.get("weight",1)
        values=dict(zip(DIGITS,PRESETS[m["pose"]])) if m.get("pose") else {}
        values.update(m.get("curl_deg",{}))
        for d,angles in values.items():
            for i,angle in enumerate(angles,1):result[f"{d}_0{i}_{m['side']}"]=(angle,weight)
    return result
