//! Naga GLES codegen regression for the PowerVR constructor failure.
//! This inspects translation only; Mesa and real hardware must still compile it.
#![cfg(all(feature = "native", feature = "gles-smoke", target_os = "linux"))]

#[test]
fn primitive_local_initialization_avoids_nested_constructors_on_gles() {
    use naga::{back::glsl, valid::{Capabilities, ValidationFlags, Validator}, ShaderStage};
    let source = include_str!("../../gs/raster.wgsl");
    let module = naga::front::wgsl::parse_str(source).expect("parse raster WGSL");
    let info = Validator::new(ValidationFlags::all(), Capabilities::all()).validate(&module).expect("validate raster");
    for key in 0..8 {
        let mut constants = naga::back::PipelineConstants::default();
        for (bit, name) in [(1, "EARLY_DEPTH"), (2, "ADDRESS_SHIFT"), (4, "EXCLUSIVE_WORD_STORE")] {
            constants.insert(name.to_string(), if key & bit == 0 { 0.0 } else { 1.0 });
        }
        let (module, info) = naga::back::pipeline_constants::process_overrides(
            &module, &info, Some((ShaderStage::Compute, "main")), &constants,
        ).expect("apply raster override constants");
        let pipeline = glsl::PipelineOptions { shader_stage: ShaderStage::Compute, entry_point: "main".into(), multiview: None };
        for version in [glsl::Version::new_gles(310), glsl::Version::new_gles(320), glsl::Version::Desktop(450)] {
            let options = glsl::Options { version, zero_initialize_workgroup_memory: false, ..Default::default() };
            let mut output = String::new();
            glsl::Writer::new(&mut output, &module, &info, &options, &pipeline, Default::default())
                .expect("GLSL writer").write().expect("GLSL translation");
            if matches!(version, glsl::Version::Embedded { .. }) {
                assert!(!output.contains("Primitive("), "key {key}: nested struct constructor remains on GLES");
                for line in ["d.position[0] = uvec4(0u);", "d.position[2] = uvec4(0u);",
                             "d.uv[2] = uvec4(0u);", "d.stq[2] = vec4(0.0);",
                             "d.info = uvec4(0u);", "d.bounds = uvec4(0u);", "d.depth[1] = uvec4(0u);"] {
                    assert!(output.contains(line), "key {key}: missing zero initialization {line}");
                }
            } else {
                assert!(output.contains("Primitive("), "desktop GLSL should stay unchanged");
            }
        }
    }
}

#[test]
fn invariant_position_only_on_vertex_output_in_gles() {
    use naga::{back::glsl, valid::{Capabilities, ValidationFlags, Validator}, ShaderStage};
    // As in native/init.rs: with clip-space w=1, replace noperspective by
    // perspective only on GL. Test the actual game shader, not a mock.
    let shader = include_str!("../../gs/native.wgsl")
        .replace("@location(0) @interpolate(linear) stqf", "@location(0) @interpolate(perspective) stqf")
        .replace("@location(1) @interpolate(linear) color", "@location(1) @interpolate(perspective) color");
    let module = naga::front::wgsl::parse_str(&shader).expect("parse native WGSL");
    let info = Validator::new(ValidationFlags::all(), Capabilities::all()).validate(&module).expect("validate native WGSL");
    for version in [glsl::Version::new_gles(310), glsl::Version::new_gles(320), glsl::Version::Desktop(450)] {
        for (stage, entry) in [(ShaderStage::Vertex, "native_vertex"), (ShaderStage::Fragment, "native_pixel"),
                               (ShaderStage::Fragment, "native_pixel_blend")] {
            let pipeline = glsl::PipelineOptions { shader_stage: stage, entry_point: entry.into(), multiview: None };
            let options = glsl::Options { version, ..Default::default() };
            let mut output = String::new();
            glsl::Writer::new(&mut output, &module, &info, &options, &pipeline, Default::default())
                .expect("GLSL writer").write().expect("translate native shader");
            if stage == ShaderStage::Vertex {
                assert!(output.contains("invariant gl_Position;"), "{version:?} must preserve vertex invariance");
            } else if matches!(version, glsl::Version::Embedded { .. }) {
                assert!(!output.contains("invariant gl_FragCoord;"), "{version:?} fragment input must not be invariant");
            } else {
                assert!(output.contains("invariant gl_FragCoord;"), "desktop output unchanged");
            }
        }
    }
}
