"""Original public13/18 texture operations with CU typed async completion."""
import unreal
import toolset_registry
from .async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.operations import volume_texture


@unreal.uclass()
class CubelessVolumeTextureTools(unreal.ToolsetDefinition):
    """Native jobs retain completion/source-volume save prefixes.

    Return standard11 after actual Native terminal and durable journal completion.
    Ordinary synchronous Batch integration is pending. No public job/status/cancel
    or wait/timeout fields are added.
    """

    @toolset_registry.tool_call
    @staticmethod
    def create_volume_texture_from_2d_sheet_mcp(source_texture_path: str, target_volume_texture_path: str, tile_size_x: int = 128, tile_size_y: int = 128, dry_run: bool = False, overwrite_existing: bool = False, save_asset: bool = True, srgb: bool | None = None, compression_settings: str = 'TC_VectorDisplacementmap', mip_gen_settings: str = 'TMGS_NoMipmaps', never_stream: bool = True, virtual_texture_streaming: bool = False, address_mode: str = 'TA_Wrap') -> CubelessPythonAsyncOperationResult:
        """Original sheet13; srgb omitted/null inherits source SRGB. Native asynchronous build precedes conditional Volume save; actual update/create/save/sizes/failure prefix remains authoritative."""
        return volume_texture.begin('create_volume_texture_from_2d_sheet_mcp', dict(source_texture_path=source_texture_path, target_volume_texture_path=target_volume_texture_path, tile_size_x=tile_size_x, tile_size_y=tile_size_y, dry_run=dry_run, overwrite_existing=overwrite_existing, save_asset=save_asset, srgb=srgb, compression_settings=compression_settings, mip_gen_settings=mip_gen_settings, never_stream=never_stream, virtual_texture_streaming=virtual_texture_streaming, address_mode=address_mode))

    @toolset_registry.tool_call
    @staticmethod
    def create_volume_texture_from_raw_rgba16_sheet_mcp(raw_file_path: str, source_texture_path: str, target_volume_texture_path: str, sheet_size_x: int = 2048, sheet_size_y: int = 1024, tile_size_x: int = 128, tile_size_y: int = 128, dry_run: bool = False, overwrite_existing: bool = False, save_assets: bool = True, srgb: bool = False, source_compression_settings: str = 'TC_Default', volume_compression_settings: str = 'TC_VectorDisplacementmap', source_mip_gen_settings: str = 'TMGS_FromTextureGroup', volume_mip_gen_settings: str = 'TMGS_NoMipmaps', never_stream_volume: bool = True, virtual_texture_streaming: bool = False, address_mode: str = 'TA_Wrap') -> CubelessPythonAsyncOperationResult:
        """Original rawRGBA18; exact RGBA16 source initialization precedes Volume update. Completion precedes original source-save then volume-save, with both returned flags retained even partial failure. No wrapper second save or atomic rollback."""
        return volume_texture.begin('create_volume_texture_from_raw_rgba16_sheet_mcp', dict(raw_file_path=raw_file_path, source_texture_path=source_texture_path, target_volume_texture_path=target_volume_texture_path, sheet_size_x=sheet_size_x, sheet_size_y=sheet_size_y, tile_size_x=tile_size_x, tile_size_y=tile_size_y, dry_run=dry_run, overwrite_existing=overwrite_existing, save_assets=save_assets, srgb=srgb, source_compression_settings=source_compression_settings, volume_compression_settings=volume_compression_settings, source_mip_gen_settings=source_mip_gen_settings, volume_mip_gen_settings=volume_mip_gen_settings, never_stream_volume=never_stream_volume, virtual_texture_streaming=virtual_texture_streaming, address_mode=address_mode))
