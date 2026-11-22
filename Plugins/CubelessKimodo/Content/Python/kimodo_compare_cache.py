"""Build an editor-only SOMA geometry cache from the original BVH."""
import sys
from pathlib import Path


def skin(bvh, directory):
    import numpy as np
    import torch
    import runpy
    import kimodo
    from kimodo.exports.bvh import bvh_to_kimodo_motion
    from kimodo.skeleton.registry import build_skeleton

    torch.set_num_threads(2)
    skeleton = build_skeleton(77)
    motion, _ = bvh_to_kimodo_motion(bvh, skeleton, standard_tpose=True)
    skin_type = runpy.run_path(str(Path(kimodo.__file__).parent / "viz/soma_skin.py"))["SOMASkin"]
    model = skin_type(skeleton)
    rotations, positions = motion["global_rot_mats"], motion["posed_joints"]
    vertices = np.lib.format.open_memmap(directory/"vertices.npy", mode="w+", dtype="float32",
                                         shape=(len(rotations), len(model.lbs_indices), 3))
    with torch.no_grad():
        for i in range(0,len(rotations),4):
            chunk = model.skin(rotations[i:i+4], positions[i:i+4], True).cpu().numpy()
            vertices[i:i+len(chunk)] = chunk[:,:,[2,1,0]] * [-100,-100,100]
    np.save(directory/"faces.npy", model.faces.cpu().numpy()[:,::-1])


def export(directory):
    import bpy
    import numpy as np
    vertices = np.load(directory/"vertices.npy", mmap_mode="r")
    faces = np.load(directory/"faces.npy")
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    mesh = bpy.data.meshes.new("SOMA")
    mesh.from_pydata(vertices[0].tolist(), [], faces.tolist())
    obj = bpy.data.objects.new("SOMA",mesh)
    scene.collection.objects.link(obj)
    for face in mesh.polygons:
        face.use_smooth = True
    # Shape keys flag the mesh as time-varying for the Alembic exporter.
    obj.shape_key_add(name="Basis")
    for f in range(1,len(vertices)):
        key = obj.shape_key_add(name=str(f))
        key.data.foreach_set("co",vertices[f].ravel())
        for frame,weight in ((f-1,0),(f,1),(f+1,0)):
            key.value = weight
            key.keyframe_insert(data_path="value",frame=frame)
    scene.render.fps = 30
    scene.frame_start,scene.frame_end = 0,len(vertices)-1
    bpy.ops.wm.alembic_export(filepath=str(directory/"Soma.abc"),start=0,end=len(vertices)-1,uvs=False,normals=True)


if __name__ == "__main__":
    if "--" in sys.argv:
        export(Path(sys.argv[-1]))
    else:
        skin(Path(sys.argv[1]),Path(sys.argv[2]))
