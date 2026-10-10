"""Disabled compatibility shim for legacy API-key image generation imports.

Project policy requires texture source art to use Codex built-in image
generation or local/procedural generation, not API-key based image calls.
Keep these helpers so old imports fail safely instead of sending network calls.
"""

import re
from pathlib import Path
from typing import Any, Dict, Optional

from tools.response_schema import tool_error


def sanitize_output_name(output_name: str) -> str:
    """Return a filesystem-friendly PNG basename without an extension."""
    name = re.sub(r"[^A-Za-z0-9_.-]+", "_", output_name.strip())
    name = name.strip("._")
    return name or "AI_Texture"


def ensure_png_path(output_dir: str, output_name: str) -> str:
    """Build an absolute PNG path and create its parent directory."""
    safe_name = sanitize_output_name(output_name)
    output_path = Path(output_dir).expanduser().resolve() / f"{safe_name}.png"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    return str(output_path)


def generate_texture_png(
    prompt: str,
    output_path: str,
    size: str = "1024x1024",
    reference_image_path: Optional[str] = None,
    model: Optional[str] = None,
) -> Dict[str, Any]:
    """Return a structured refusal instead of calling an API-key image route."""
    return tool_error(
        (
            "API-key based image generation is disabled for this project. "
            "Use Codex built-in image generation to create the PNG, then import "
            "the generated file through UnrealMCP texture import tools."
        ),
        error_code="api_image_generation_disabled",
        stage="builtin_image_generation_required",
        api_route_disabled=True,
        billing_route="none",
        image_generation_route="codex_builtin_image_generation",
        prompt=prompt,
        expected_output_path=output_path,
        requested_size=size,
        reference_image_path=reference_image_path or "",
        requested_model_ignored=model or "",
    )
