"""Original Editor status UI; independent of the removed TCP status probe."""
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import status_window


@unreal.uclass()
class CubelessStatusWindowTools(unreal.ToolsetDefinition):
    """One owned nonmodal Editor status window; no game runtime dependency."""

    @toolset_registry.tool_call
    @staticmethod
    def show_ieta_connection_status(text: str = "흥, Unreal MCP 연결은 확인했어.", command: str = "이에타", progress: float = 1.0) -> CubelessPythonOperationResult:
        """Display/update the owned Slate status window and schedule close after five seconds. No UI automation, asset write, compile or save. Unattended/non-rendering sessions refuse before window creation. The original arbitrary/default command-label filtering defect is explicitly repaired so the requested text can be displayed. Progress remains accepted/clamped input only: the original handler never applied it. Editor API response and Slate request receipts do not verify TCP, the official server connection, pixels, avatar rendering or transport failure equivalence. The old avatar asset/theme and global processing/log-status integration are not migrated by this small public UI operation.

        Args:
            text: Original default message; empty uses the original handler fallback.
            command: Original label default; empty becomes ieta_status. No port/server interpretation.
            progress: Original numeric default1; clamp0..1 only, with progress_applied=false.
        """
        return reflected_result(status_window.show_ieta_connection_status(text, command, progress))
