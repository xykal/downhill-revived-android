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
    let objects = Objects::create(gpu, false).expect("native renderer objects on GL");
    // Both present pipelines compile on a real GLES driver; FXAA is lazy in production.
    let _ = objects.present_pipe(gpu, wgpu::TextureFormat::Rgba8Unorm, true);
    check_fxaa(gpu, &objects);
    let _native = dhd_render::native::Native::create(gpu, 0).expect("native startup and probe pipelines on GL");
    check_depth_load(gpu);
    check_nested_struct_zero_init(gpu);
    for key in 0..8 {
        dhd_render::exact::pipelines::Pipelines::create(gpu, key).unwrap_or_else(|e| panic!("exact key {key}: {e}"));
    }
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

fn check_nested_struct_zero_init(gpu: &Gpu) {
    let d = &gpu.device;
    let result = d.create_buffer(&wgpu::BufferDescriptor {
        label: None, size: 8, usage: wgpu::BufferUsages::STORAGE | wgpu::BufferUsages::COPY_SRC, mapped_at_creation: false,
    });
    let read = d.create_buffer(&wgpu::BufferDescriptor {
        label: None, size: 8, usage: wgpu::BufferUsages::COPY_DST | wgpu::BufferUsages::MAP_READ, mapped_at_creation: false,
    });
    let module = d.create_shader_module(wgpu::ShaderModuleDescriptor {
        label: Some("nested zero-init test"), source: wgpu::ShaderSource::Wgsl(r#"
struct Child { v: vec4<u32>, n: i32 }
struct Outer { a: array<Child, 2>, b: array<vec4<u32>, 3>, f: f32 }
@group(0) @binding(0) var<storage, read_write> output: array<u32>;
@compute @workgroup_size(1)
fn main() {
    var zero: Outer;
    output[0] = zero.a[0].v.x | zero.a[1].v.w | u32(zero.a[0].n) | u32(zero.a[1].n)
        | zero.b[0].x | zero.b[1].y | zero.b[2].z | bitcast<u32>(zero.f);
    output[1] = 0xCAFECAFEu;
}
"#.into()),
    });
    let pipe = d.create_compute_pipeline(&wgpu::ComputePipelineDescriptor {
        label: None, layout: None, module: &module, entry_point: Some("main"),
        compilation_options: Default::default(), cache: None,
    });
    let group = d.create_bind_group(&wgpu::BindGroupDescriptor {
        label: None, layout: &pipe.get_bind_group_layout(0),
        entries: &[wgpu::BindGroupEntry { binding: 0, resource: result.as_entire_binding() }],
    });
    let mut encoder = d.create_command_encoder(&Default::default());
    {
        let mut pass = encoder.begin_compute_pass(&Default::default());
        pass.set_pipeline(&pipe);
        pass.set_bind_group(0, &group, &[]);
        pass.dispatch_workgroups(1, 1, 1);
    }
    encoder.copy_buffer_to_buffer(&result, 0, &read, 0, 8);
    gpu.queue.submit([encoder.finish()]);
    let (tx, rx) = std::sync::mpsc::channel();
    read.slice(..).map_async(wgpu::MapMode::Read, move |r| tx.send(r).unwrap());
    d.poll(wgpu::PollType::wait_indefinitely()).unwrap();
    rx.recv().unwrap().unwrap();
    let bytes = read.slice(..).get_mapped_range();
    assert_eq!(u32::from_le_bytes(bytes[0..4].try_into().unwrap()), 0);
    assert_eq!(u32::from_le_bytes(bytes[4..8].try_into().unwrap()), 0xCAFECAFE);
    drop(bytes);
    read.unmap();
}

// Render a black/white staircase at 1:1. FXAA must preserve flat regions and produce
// intermediate shades along an edge. This is a software GLES correctness smoke test,
// not a visual-quality or speed test on PowerVR.
fn check_fxaa(gpu: &Gpu, objects: &Objects) {
    let d = &gpu.device;
    const W: u32 = 16;
    let texture = |usage| d.create_texture(&wgpu::TextureDescriptor {
        label: None, size: wgpu::Extent3d { width: W, height: W, depth_or_array_layers: 1 },
        mip_level_count: 1, sample_count: 1, dimension: wgpu::TextureDimension::D2,
        format: wgpu::TextureFormat::Rgba8Unorm, usage, view_formats: &[],
    });
    let src = texture(wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST);
    let dst = texture(wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::COPY_SRC);
    let mut pixels = vec![0u8; (W * W * 4) as usize];
    for y in 0..W { for x in 0..W {
        let i = ((y * W + x) * 4) as usize;
        let c = if x >= y { 255 } else { 0 };
        pixels[i..i+4].copy_from_slice(&[c,c,c,255]);
    }}
    gpu.queue.write_texture(wgpu::TexelCopyTextureInfo { texture: &src, mip_level: 0,
        origin: wgpu::Origin3d::ZERO, aspect: wgpu::TextureAspect::All }, &pixels,
        wgpu::TexelCopyBufferLayout { offset: 0, bytes_per_row: Some(W * 4), rows_per_image: Some(W) },
        wgpu::Extent3d { width: W, height: W, depth_or_array_layers: 1 });
    let sampler = d.create_sampler(&wgpu::SamplerDescriptor { mag_filter: wgpu::FilterMode::Linear,
        min_filter: wgpu::FilterMode::Linear, ..Default::default() });
    let view = src.create_view(&Default::default());
    let texture_group = d.create_bind_group(&wgpu::BindGroupDescriptor { label: None,
        layout: &objects.texture_layout, entries: &[
            wgpu::BindGroupEntry { binding: 0, resource: wgpu::BindingResource::TextureView(&view) },
            wgpu::BindGroupEntry { binding: 1, resource: wgpu::BindingResource::Sampler(&sampler) },
        ] });
    let uniform = d.create_buffer(&wgpu::BufferDescriptor { label: None, size: 256,
        usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST, mapped_at_creation: false });
    let mut rect = Vec::new();
    for v in [0.0f32, 0.0, W as f32, W as f32] { rect.extend_from_slice(&v.to_le_bytes()); }
    gpu.queue.write_buffer(&uniform, 0, &rect);
    let uniform_group = d.create_bind_group(&wgpu::BindGroupDescriptor { label: None,
        layout: &objects.uniform_layout, entries: &[wgpu::BindGroupEntry { binding: 0,
            resource: wgpu::BindingResource::Buffer(wgpu::BufferBinding { buffer: &uniform,
                offset: 0, size: wgpu::BufferSize::new(16) }) }] });
    let read = d.create_buffer(&wgpu::BufferDescriptor { label: None, size: 256 * W as u64,
        usage: wgpu::BufferUsages::COPY_DST | wgpu::BufferUsages::MAP_READ, mapped_at_creation: false });
    let mut encoder = d.create_command_encoder(&Default::default());
    let target = dst.create_view(&Default::default());
    {
        let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor { label: None,
            color_attachments: &[Some(wgpu::RenderPassColorAttachment { view: &target,
                depth_slice: None, resolve_target: None, ops: wgpu::Operations {
                    load: wgpu::LoadOp::Clear(wgpu::Color::BLACK), store: wgpu::StoreOp::Store } })],
            ..Default::default() });
        pass.set_pipeline(objects.present_pipe(gpu, wgpu::TextureFormat::Rgba8Unorm, true));
        pass.set_bind_group(0, &uniform_group, &[0]);
        pass.set_bind_group(1, &texture_group, &[]);
        pass.draw(0..3, 0..1);
    }
    encoder.copy_texture_to_buffer(wgpu::TexelCopyTextureInfo { texture: &dst, mip_level: 0,
        origin: wgpu::Origin3d::ZERO, aspect: wgpu::TextureAspect::All },
        wgpu::TexelCopyBufferInfo { buffer: &read, layout: wgpu::TexelCopyBufferLayout {
            offset: 0, bytes_per_row: Some(256), rows_per_image: Some(W) } },
        wgpu::Extent3d { width: W, height: W, depth_or_array_layers: 1 });
    gpu.queue.submit([encoder.finish()]);
    let (tx, rx) = std::sync::mpsc::channel();
    read.slice(..).map_async(wgpu::MapMode::Read, move |r| tx.send(r).unwrap());
    d.poll(wgpu::PollType::wait_indefinitely()).unwrap();
    rx.recv().unwrap().unwrap();
    let bytes = read.slice(..).get_mapped_range();
    assert_eq!(bytes[15 * 4], 255, "flat white must stay white");
    assert_eq!(bytes[15 * 256], 0, "flat black must stay black");
    let mixed = (0..W as usize).flat_map(|y| (0..W as usize).map(move |x| y * 256 + x * 4))
        .filter(|&i| bytes[i] > 0 && bytes[i] < 255).count();
    assert!(mixed > 0, "FXAA must produce intermediate edge pixels");
    eprintln!("FXAA smoke: {mixed} intermediate edge pixels");
    drop(bytes);
    read.unmap();
}
