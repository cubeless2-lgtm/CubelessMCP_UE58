"""Offline adversarial checks for Actor runtime manifest/identity boundaries."""
import copy
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parent))
import actor_runtime_manifest as contract
from validate_actor_runtime_fixture import check_dependencies


class ActorRuntimeContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest=contract.read_json(contract.RECORDS/"operations-actor-runtime-manifest-A.json")

    def setUp(self): self.m=copy.deepcopy(self.manifest)

    def rows(self,marker=False):
        rows=copy.deepcopy(self.m["owned_actors"]+[self.m["protected_actor"]])
        if marker: rows.append({"name":"BP_RuntimeProof_C_0","class_path":self.m["marker_actor_class"],"actor_path":contract.object_path(self.m["source_map"])+":PersistentLevel.BP_RuntimeProof_C_0","label":contract.MARKER_LABEL,"location":[500,0,0],"rotation":[0,0,0],"scale":[1,1,1]})
        return rows

    def verify_rows(self,rows,marker=False):return contract.actor_rows(rows,self.m,self.m["source_map"],marker_required=marker)

    def test_actual_parent_evidence_rederives(self):self.assertTrue(contract.validate(self.m))
    def test_destination_path_tamper_rejected(self):
        self.m["files"][0]["destination"]=str(contract.ROOT/"Content/UserMap.umap")
        with self.assertRaises(RuntimeError):contract.validate(self.m)
    def test_default_map_tamper_rejected(self):
        self.m["default_map"]="/Game/UserMap"
        with self.assertRaises(RuntimeError):contract.validate(self.m)
    def test_five_owned_and_protected(self):self.assertEqual(len(self.verify_rows(self.rows())),6)
    def test_marker_and_exact_remap(self):
        rows=self.verify_rows(self.rows(True),True);mapped=contract.remap_rows(rows,self.m["source_map"],self.m["default_map"])
        self.assertEqual(contract.actor_rows(mapped,self.m,self.m["default_map"],marker_required=True),mapped)
    def test_wrong_world_remap_rejected(self):
        rows=self.rows();rows[0]["actor_path"]=rows[0]["actor_path"].replace("/Game/","/Other/")
        with self.assertRaises(RuntimeError):contract.remap_rows(rows,self.m["source_map"],self.m["default_map"])
    def test_actor_class_tamper(self):
        rows=self.rows();rows[0]["class_path"]="/Script/Engine.CameraActor"
        with self.assertRaises(RuntimeError):self.verify_rows(rows)
    def test_nonfinite_transform(self):
        rows=self.rows();rows[0]["location"][0]=float("nan")
        with self.assertRaises(RuntimeError):self.verify_rows(rows)
    def test_boolean_transform(self):
        rows=self.rows();rows[0]["rotation"][0]=False
        with self.assertRaises(RuntimeError):self.verify_rows(rows)
    def test_transform_tamper(self):
        rows=self.rows();rows[0]["location"][0]+=1
        with self.assertRaises(RuntimeError):self.verify_rows(rows)
    def test_deleted_camera_returns(self):
        rows=self.rows();camera=copy.deepcopy(rows[0]);camera["name"]=self.m["deleted_actor_name"];rows.append(camera)
        with self.assertRaises(RuntimeError):self.verify_rows(rows)
    def test_duplicate_actor(self):
        rows=self.rows();rows.append(copy.deepcopy(rows[0]))
        with self.assertRaises(RuntimeError):self.verify_rows(rows)
    def test_missing_actor(self):
        with self.assertRaises(RuntimeError):self.verify_rows(self.rows()[1:])
    def test_marker_impostor(self):
        rows=self.rows(True);rows[-1]["class_path"]="/Script/Engine.Actor"
        with self.assertRaises(RuntimeError):self.verify_rows(rows,True)
    def test_marker_spawn_transform(self):
        rows=self.rows(True);rows[-1]["location"][0]=0
        with self.assertRaises(RuntimeError):self.verify_rows(rows,True)
    def test_unrelated_system_actor_permitted_raw(self):
        rows=self.rows();rows.append({"name":"EngineTransient","class_path":"/Script/Engine.Actor","actor_path":"irrelevant","label":"EngineTransient"})
        self.assertEqual(len(self.verify_rows(rows)),6)
    def graph(self):
        marker=next(r["package"] for r in self.m["files"] if r["role"]=="actor");save=next(r["package"] for r in self.m["files"] if r["role"]=="save_game")
        return {"complete":True,"passed_within_declared_scope":True,"forbidden_references":[],"packages":[{"package":p} for p in self.m["required_cooked_packages"]],"edges":[{"from":self.m["default_map"],"to":marker,"category":"hard_package"},{"from":marker,"to":save,"category":"hard_package"}],"native_script_terminals":["/Script/Engine"]}
    def test_expected_dependency_chain(self):self.assertTrue(check_dependencies(self.m,self.graph())["passed"])
    def test_engine_script_terminal_not_guaranteed(self):
        graph=self.graph();graph["native_script_terminals"]=["/Script/NavigationSystem"]
        result=check_dependencies(self.m,graph)
        self.assertTrue(result["passed"]);self.assertEqual(result["observed_native_script_terminals"],["/Script/NavigationSystem"])
    def test_missing_marker_hard_edge(self):
        graph=self.graph();graph["edges"].pop()
        with self.assertRaises(RuntimeError):check_dependencies(self.m,graph)
    def test_old_map_dependency(self):
        graph=self.graph();graph["packages"].append({"package":self.m["source_map"]})
        with self.assertRaises(RuntimeError):check_dependencies(self.m,graph)
    def test_mcp_reference(self):
        graph=self.graph();graph["forbidden_references"]=["/Script/CubelessMCPUE58Editor"]
        with self.assertRaises(RuntimeError):check_dependencies(self.m,graph)


if __name__=="__main__":unittest.main()
