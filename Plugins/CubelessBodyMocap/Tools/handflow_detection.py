"""Conservative, image-backed recovery of brief HandFlow detection gaps.

Only low-score YOLO boxes corroborated by both temporal anchors and the own
ViTPose wrist are accepted. Never relabel a box, replace an existing observation,
extrapolate a long occlusion, or use another estimator's hand pose.
"""
import numpy as np


def recover(boxes, valid, scores, candidates, wrists, fps):
    boxes, valid, scores = boxes.copy(), valid.copy(), scores.copy()
    anchors = valid.copy()  # Recovered observations cannot support more recovery.
    recovered = np.zeros_like(valid)
    horizon = max(1, int(round(float(fps)*.35)))
    for frame, (weak_boxes, weak_scores, classes) in candidates.items():
        for side in range(2):
            if valid[frame, side] or wrists[frame, side, 2] < .5:
                continue
            earlier = np.flatnonzero(anchors[max(0, frame-horizon):frame, side])
            later = np.flatnonzero(anchors[frame+1:frame+horizon+1, side])
            if not len(earlier) or not len(later):
                continue
            a = max(0, frame-horizon)+earlier[-1]
            z = frame+1+later[0]
            t = (frame-a)/(z-a)
            # Predict only box size/offset from the tracked wrist, allowing fast
            # translation of the whole hand. No pose or rotation interpolation.
            size_a, size_z = boxes[a, side, 2:]-boxes[a, side, :2], boxes[z, side, 2:]-boxes[z, side, :2]
            size = size_a*(1-t)+size_z*t
            offset_a = boxes[a, side].reshape(2, 2).mean(0)-wrists[a, side, :2]
            offset_z = boxes[z, side].reshape(2, 2).mean(0)-wrists[z, side, :2]
            expected = wrists[frame, side, :2]+offset_a*(1-t)+offset_z*t
            choices = []
            for box, score, cls in zip(weak_boxes, weak_scores, classes):
                if int(cls) != side or not .15 <= score < .3:
                    continue
                extent = box[2:]-box[:2]
                if np.any(extent < size*.5) or np.any(extent > size*2):
                    continue
                center = box.reshape(2, 2).mean(0)
                radius = max(float(np.linalg.norm(extent)), 12.)
                own = np.linalg.norm(center-wrists[frame, side, :2])
                other = np.linalg.norm(center-wrists[frame, 1-side, :2])
                # Require a positive ownership margin even during hand overlap.
                if own > radius*.7 or other-own < radius*.12:
                    continue
                distance = np.linalg.norm(center-expected)/max(float(np.linalg.norm(size)), 12.)
                if distance > .85:
                    continue
                # Do not claim the same image region already used by the other hand.
                if valid[frame, 1-side]:
                    other_box = boxes[frame, 1-side]
                    overlap = np.maximum(0, np.minimum(box[2:], other_box[2:])-np.maximum(box[:2], other_box[:2])).prod()
                    union = extent.prod()+(other_box[2:]-other_box[:2]).prod()-overlap
                    if overlap/max(float(union), 1.) > .35:
                        continue
                choices.append((distance-float(score)*.1, box, score))
            if choices:
                _, box, score = min(choices, key=lambda x:x[0])
                boxes[frame, side], scores[frame, side] = box, score
                valid[frame, side] = recovered[frame, side] = True
    return boxes, valid, scores, recovered
