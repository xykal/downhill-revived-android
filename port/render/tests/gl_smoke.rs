//! Mesa GLES regression: compile startup pipelines and verify depth textureLoad.
//! LIBGL_ALWAYS_SOFTWARE=1 DHD_TEST_FORCE_GLES=1 cargo test --locked --features native,gles-smoke --test gl_smoke -- --nocapture
//! This does not test Android window creation, PowerVR drivers, or gameplay.
#![cfg(all(feature = "native", feature = "gles-smoke", target_os = "linux"))]

mod common;
use dhd_render::abi::{DHDR_DEV_GL, DHDR_LOG_WARN};
use dhd_render::device::{DeviceOptions, Gpu};
use dhd_render::native::init::Objects;

#[test]
fn gles_native_startup_and_depth_load() {
    // The GLES shader workaround is only valid with constant clip-space w=1.
    let native_shader = include_str!("../../gs/native.wgsl");
    assert!(native_shader.contains("position.z, 1.0);"));
    assert_eq!(native_shader.matches("@interpolate(linear)").count(), 2);
    assert!(native_shader.contains("@location(0) @interpolate(linear) stqf"));
    assert!(native_shader.contains("@location(1) @interpolate(linear) color"));
    let options = DeviceOptions { flags: DHDR_DEV_GL, log_level: DHDR_LOG_WARN, ..DeviceOptions::default() };
    let gpu: &'static Gpu = Box::leak(Box::new(Gpu::create(&options).expect("GL adapter and device")));
    assert_eq!(gpu.info.backend, wgpu::Backend::Gl);
    if std::env::var_os("DHD_TEST_FORCE_GLES").is_some() {
        assert!(gpu.info.driver_info.contains("OpenGL ES"), "not an ES context: {:?}", gpu.info);
    }
    eprintln!("GL adapter: {:?}", gpu.info);
    let _objects = Objects::create(gpu, false).expect("native renderer objects on GL");
    let _native = dhd_render::native::Native::create(gpu, 0).expect("native startup and probe pipelines on GL");
    check_depth_load(gpu);
}

fn check_depth_load(gpu: &Gpu) {
    let d = &gpu.device;
    let depth = d.create_texture(&wgpu::TextureDescriptor {
        label: Some("depth test"), size: wgpu::Extent3d { width: 4, height: 4, depth_or_array_layers: 1 },
        mip_level_count: 1, sample_count: 1, dimension: wgpu::TextureDimension::D2,
        format: wgpu::TextureFormat::Depth32Float,
        usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::TEXTURE_BINDING, view_formats: &[],
    });
    let view = depth.create_view(&Default::default());
    let result = d.create_buffer(&wgpu::BufferDescriptor {
        label: None, size: 16, usage: wgpu::BufferUsages::STORAGE | wgpu::BufferUsages::COPY_SRC, mapped_at_creation: false,
    });
    let read = d.create_buffer(&wgpu::BufferDescriptor {
        label: None, size: 16, usage: wgpu::BufferUsages::COPY_DST | wgpu::BufferUsages::MAP_READ, mapped_at_creation: false,
    });
    let shader = d.create_shader_module(wgpu::ShaderModuleDescriptor {
        label: Some("depth textureLoad test"), source: wgpu::ShaderSource::Wgsl(r#"
@group(0) @binding(0) var depth_tex: texture_depth_2d;
@group(0) @binding(1) var<storage, read_write> result: array<f32>;
@compute @workgroup_size(1)
fn main() {
    result[0] = textureLoad(depth_tex, vec2<i32>(0, 0), 0);
    result[1] = textureLoad(depth_tex, vec2<i32>(3, 3), 0);
}
"#.into()),
    });
    let pipeline = d.create_compute_pipeline(&wgpu::ComputePipelineDescriptor {
        label: None, layout: None, module: &shader, entry_point: Some("main"),
        compilation_options: Default::default(), cache: None,
    });
    let layout = pipeline.get_bind_group_layout(0);
    let group = d.create_bind_group(&wgpu::BindGroupDescriptor {
        label: None, layout: &layout,
        entries: &[
            wgpu::BindGroupEntry { binding: 0, resource: wgpu::BindingResource::TextureView(&view) },
            wgpu::BindGroupEntry { binding: 1, resource: result.as_entire_binding() },
        ],
    });
    for value in [0.0, 0.25, 0.75, 1.0] {
        let mut enc = d.create_command_encoder(&Default::default());
        drop(enc.begin_render_pass(&wgpu::RenderPassDescriptor {
            color_attachments: &[], depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment {
                view: &view, depth_ops: Some(wgpu::Operations { load: wgpu::LoadOp::Clear(value), store: wgpu::StoreOp::Store }),
                stencil_ops: None,
            }), ..Default::default()
        }));
        {
            let mut pass = enc.begin_compute_pass(&Default::default());
            pass.set_pipeline(&pipeline);
            pass.set_bind_group(0, &group, &[]);
            pass.dispatch_workgroups(1, 1, 1);
        }
        enc.copy_buffer_to_buffer(&result, 0, &read, 0, 16);
        gpu.queue.submit([enc.finish()]);
        let (tx, rx) = std::sync::mpsc::channel();
        read.slice(..).map_async(wgpu::MapMode::Read, move |r| tx.send(r).unwrap());
        d.poll(wgpu::PollType::wait_indefinitely()).unwrap();
        rx.recv().unwrap().unwrap();
        let mapped = read.slice(..).get_mapped_range();
        for offset in [0, 4] {
            let actual = f32::from_le_bytes(mapped[offset..offset + 4].try_into().unwrap());
            assert!((actual - value as f32).abs() < 0.00001, "depth load {actual} != {value}");
        }
        drop(mapped);
        read.unmap();
    }
}
