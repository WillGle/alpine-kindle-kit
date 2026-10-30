# Returning Alpine-image preparation

Separate adaptation of device-qualified native-stage. Different module/device
and launcher names; old proven artifacts remain untouched. Image limit3MiB,
allocator budget unchanged1024 pages, free-RAM guard16MiB. The fixed image is
2722513bytes, SHA256 16c9be187c4ddefe61b2b5e184b03085db557f19acc9a9191c036cfa947ab92f; DTB offset2702408, size20105.
Rounded source requirement is665pages plus control/swap/list;
the actual allocator can require additional collision pages and must decline
cleanly on budget exhaustion. No reclaim/force-load/execute interface.

Run build-host.sh then package.py on host. Device returning preparation passed78 checks in capture
`gleworks-alpine-stage-20261030-010508-14349`; see [runtime review](../../../../linux/alpine-stage/runtime-review.json).
The full image used668 owned pages; normal unload returned ownership to zero.
Do not infer native boot GO from a successful returning preparation trial.
This package does not stop DMA, arm a watchdog, reboot, modify bootmode or flash.
