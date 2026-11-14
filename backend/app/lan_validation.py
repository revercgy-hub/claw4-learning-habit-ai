"""Expose only the device routes of a synthetic backend on the validation LAN."""

import re

from starlette.responses import JSONResponse, Response

from app.main import app as family


_POST_ROUTES = {
    "/api/v1/devices/challenge",
    "/api/v1/devices/auth",
    "/api/v1/events/batch",
}
_TODAY = re.compile(r"/api/v1/children/[A-Za-z0-9_-]{1,128}/tasks/today")


async def app(scope, receive, send):
    if scope["type"] == "lifespan":
        await family(scope, receive, send)
        return
    if scope["type"] == "websocket":
        await send({"type": "websocket.close", "code": 1008})
        return
    method, path = scope["method"], scope["path"]
    if method == "GET" and path == "/health":
        await JSONResponse({"status": "ok", "service": "claw4-family-backend", "profile": "device-validation"})(
            scope, receive, send)
        return
    if ((method == "POST" and path in _POST_ROUTES) or
            (method == "GET" and _TODAY.fullmatch(path))):
        await family(scope, receive, send)
    else:
        await Response(status_code=404)(scope, receive, send)
