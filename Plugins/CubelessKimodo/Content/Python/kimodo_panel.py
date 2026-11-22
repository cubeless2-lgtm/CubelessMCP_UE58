"""Modeless Kimodo prompt window, pumped by the editor game thread."""
import json
import time
import unreal
import cubeless_kimodo as kimodo

_panel = None

class PromptPanel:
    def __init__(self):
        import tkinter as tk
        from tkinter import ttk
        self.window = tk.Tk()
        self.window.title("Kimodo · 텍스트로 모션 생성")
        self.window.geometry("510x275")
        self.window.minsize(410,255)
        self.job = None
        self.asset = None
        self.busy = False
        self.next_poll = 0
        body = ttk.Frame(self.window,padding=16)
        body.pack(fill="both",expand=True)
        ttk.Label(body,text="동작 프롬프트").pack(anchor="w")
        self.prompt = tk.Text(body,height=4,wrap="word",font=("Malgun Gothic",11),undo=True)
        self.prompt.pack(fill="both",expand=True,pady=(4,10))
        row = ttk.Frame(body)
        row.pack(fill="x")
        ttk.Label(row,text="길이(초)").pack(side="left")
        self.duration = ttk.Spinbox(row,from_=1,to=30,increment=1,width=5)
        self.duration.set("2")
        self.duration.pack(side="left",padx=(6,12))
        self.submit = ttk.Button(row,text="모션 생성",command=self.generate)
        self.submit.pack(side="left")
        self.open_button = ttk.Button(row,text="결과 열기",command=self.open_result,state="disabled")
        self.open_button.pack(side="left",padx=6)
        self.status = tk.StringVar(value="영문 예시: A person waves their right hand.")
        ttk.Label(body,textvariable=self.status,wraplength=460).pack(anchor="w",pady=(10,0))
        self.window.protocol("WM_DELETE_WINDOW",self.hide)
        self.window.update_idletasks()
        import ctypes
        hwnd = ctypes.windll.user32.GetParent(self.window.winfo_id()) or self.window.winfo_id()
        unreal.parent_external_window_to_slate(hwnd)
        self.handle = unreal.register_slate_post_tick_callback(self.tick)
        self.show()

    def show(self):
        self.window.deiconify()
        self.window.lift()
        self.prompt.focus_force()

    def hide(self):
        self.window.withdraw()

    def generate(self):
        if self.job:
            return
        try:
            prompt = self.prompt.get("1.0","end-1c").strip()
            if not prompt:
                self.status.set("생성할 동작을 입력하세요.")
                self.prompt.focus_set()
                return
            self.job = kimodo.generate(prompt,float(self.duration.get()))
            self.submit.state(["disabled"])
            self.status.set("Kimodo에서 모션을 생성하고 있습니다.")
        except Exception as error:
            self.status.set(str(error))

    def tick(self,_dt):
        if self.busy:
            return
        self.busy = True
        try:
            self.window.update()
            if self.job and time.monotonic() >= self.next_poll:
                self.next_poll = time.monotonic()+0.5
                result = json.loads(kimodo.poll(self.job))
                if result["phase"] in ("done","failed"):
                    self.job = None
                    self.submit.state(["!disabled"])
                    if result["phase"] == "done":
                        self.asset = result["asset"]
                        self.open_button.state(["!disabled"])
                        self.status.set("Quinn 애니메이션을 저장했습니다.")
                    else:
                        self.status.set(result["error"])
                elif result["phase"] == "bake":
                    self.status.set("Quinn 애니메이션으로 변환하고 있습니다.")
        finally:
            self.busy = False

    def open_result(self):
        asset = unreal.load_asset(self.asset)
        unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([asset])

def show():
    global _panel
    if _panel is None:
        _panel = PromptPanel()
    else:
        _panel.show()
    return _panel

def register_menu():
    menus = unreal.ToolMenus.get()
    entry = unreal.ToolMenuEntry(name="CubelessKimodoPrompt",type=unreal.MultiBlockType.MENU_ENTRY)
    entry.set_label("Kimodo · 텍스트로 모션 생성")
    entry.set_string_command(unreal.ToolMenuStringCommandType.PYTHON,"","import kimodo_panel; kimodo_panel.show()")
    menus.extend_menu("LevelEditor.MainMenu.Window").add_menu_entry("WindowLayout",entry)
    menus.refresh_all_widgets()
