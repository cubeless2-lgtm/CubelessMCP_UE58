"""Original logical viewport0/2/8/1 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import viewport_capture as operation

@unreal.uclass()
class CubelessViewportCaptureTools(unreal.ToolsetDefinition):
    'Active rendered Editor viewport APIs: bookmark list, review realtime override, composite bookmark/camera/clean PNG capture and simple PNG take. Public MCP preprocessing/coercion, direct Python body normalization and raw Native command presence remain distinct. PNG writes differ from asset saves; actual partial bookmark/camera/flags/override/directory/file/dirty receipts survive failure. ReadPixels has internal render flush; explicit asset compiler/save0, no automatic retry or atomic rollback.'
    @toolset_registry.tool_call
    @staticmethod
    def list_viewport_bookmarks()->CubelessPythonOperationResult:
        'List active viewport bookmark slots/existence and current camera without jump/creation or mutation journal. Existence does not prove a custom bookmark action can move the viewport.'
        rejected,report,raw=_guard_admission('list_viewport_bookmarks')
        if rejected is not None:return reflected_result(rejected)
        values={}
        return reflected_result(operation._named_mcp('list_viewport_bookmarks',values))

    @toolset_registry.tool_call
    @staticmethod
    def set_active_viewport_realtime(realtime:str='true',realtime_frame_count:str='300')->CubelessPythonOperationResult:
        'Set/remove only the MCPWeather Viewport Review realtime override and request additional frames using original defaults. Other override owners/previous frame requests remain distinct; preserve actual UI prefix and unknown effects.'
        rejected,report,raw=_guard_admission('set_active_viewport_realtime')
        if rejected is not None:return reflected_result(rejected)
        try:values={'realtime':_decode_value(realtime),'realtime_frame_count':_decode_value(realtime_frame_count)}
        except Exception as error:return reflected_result(_carrier_rejected('set_active_viewport_realtime',error,report,raw))
        return reflected_result(operation._named_mcp('set_active_viewport_realtime',values))

    @toolset_registry.tool_call
    @staticmethod
    def capture_viewport_bookmark_screenshot(filepath:str,bookmark_index:str='-1',redraw_count:str='2',clean_game_view:str='false',view_location:str='null',view_rotation:str='null',view_fov:str='null',restore_view_after_capture:str='true')->CubelessPythonOperationResult:
        'Capture PNG after optional bookmark jump and temporary camera/clean overrides. Restore temporary camera to post-jump state only, restore clean flags separately. Preserve exact original MCP schema/coercions and direct/raw differences, PNG bytes/dimensions/alpha and actual partial directory/file/UI/dirty receipts. ReadPixels internally flushes render work. No asset save/compile or automatic replay.'
        rejected,report,raw=_guard_admission('capture_viewport_bookmark_screenshot')
        if rejected is not None:return reflected_result(rejected)
        try:values={'filepath':_decode_value(filepath),'bookmark_index':_decode_value(bookmark_index),'redraw_count':_decode_value(redraw_count),'clean_game_view':_decode_value(clean_game_view),'view_location':_decode_value(view_location),'view_rotation':_decode_value(view_rotation),'view_fov':_decode_value(view_fov),'restore_view_after_capture':_decode_value(restore_view_after_capture)}
        except Exception as error:return reflected_result(_carrier_rejected('capture_viewport_bookmark_screenshot',error,report,raw))
        return reflected_result(operation._named_mcp('capture_viewport_bookmark_screenshot',values))

    @toolset_registry.tool_call
    @staticmethod
    def take_screenshot(filepath:str)->CubelessPythonOperationResult:
        'Simple active viewport PNG capture; original filepath-only successful leaf remains. No composite helper mkdir/path normalization/redraw/bookmark/camera/clean behavior is added. Actual file write is separate from asset save; no automatic retry.'
        rejected,report,raw=_guard_admission('take_screenshot')
        if rejected is not None:return reflected_result(rejected)
        try:values={'filepath':_decode_value(filepath)}
        except Exception as error:return reflected_result(_carrier_rejected('take_screenshot',error,report,raw))
        return reflected_result(operation._named_mcp('take_screenshot',values))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessViewportCaptureAttachmentLibrary', None), 'verify_viewport_capture_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact ViewportCapture adapter raw-input admission helper is unavailable')
        raw = helper(CubelessViewportCaptureTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_viewport_capture_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded ViewportCapture adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = operation._rejected(method,operation.OperationError('viewport_capture_raw_guard_admission_missing', message))
    result['stage'] = 'viewport_capture_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=operation._rejected(method,operation.OperationError('viewport_capture_registration_carrier_invalid',str(error)))
    result['stage']='viewport_capture_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
