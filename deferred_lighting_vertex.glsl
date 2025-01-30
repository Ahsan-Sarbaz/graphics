#version 460

layout(location = 0) out vec2 uv;

void main() {
	vec4 positions[4] = {
		vec4(-1.0, -1.0, 0.0, 1.0),
		vec4(1.0, -1.0, 0.0, 1.0),
		vec4(1.0, 1.0, 0.0, 1.0),
		vec4(-1.0, 1.0, 0.0, 1.0)
	};

	vec2 tex_coords[4] = {
		vec2(0.0, 0.0),
		vec2(1.0, 0.0),
		vec2(1.0, 1.0),
		vec2(0.0, 1.0)
	};

	gl_Position = positions[gl_VertexID];
	uv = tex_coords[gl_VertexID];
}
