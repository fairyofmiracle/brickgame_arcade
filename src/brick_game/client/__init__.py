"""
REST client for BrickGame console/desktop interfaces.

The implementation is intentionally small and follows the server API:
- POST /games/{gameId}
- POST /actions
- GET /state
"""

from .api import ActionId, ApiConfig, BrickGameApiClient

__all__ = ["ActionId", "ApiConfig", "BrickGameApiClient"]
