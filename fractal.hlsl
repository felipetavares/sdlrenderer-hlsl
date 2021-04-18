sampler2D tex: register(S0);

#define csq(a) float2((a).x*(a).x-(a).y*(a).y, (a).x*(a).y*2)

int mandelbrot(float2 c) {
    float2 z = float2(0.0, 0.0);

    for(int i=1;i<14;i++) {
        z = csq(z)+c;

        if (dot(z, z) > 4.0) {
            return i;
        }
    }

    return 14;
}

float sup(float n, float4 hsla) {
	float k = (n+hsla.x*12.0)%12;
	float a = hsla.y*min(hsla.z, 1-hsla.z);
	return hsla.z-a*max(-1, min(k-3, min(9-k, 1)));
}

float4 rgba(float4 hsla) {
	return float4(sup(0.0, hsla), sup(8.0, hsla), sup(4.0, hsla), hsla.a);
}

float4 main(float2 uv: TEXCOORD): SV_Target {
	float mandelbrot_color = mandelbrot((uv-float2(0.70, 0.5))*4)/14.0;

	float frame = tex2D(tex, uv).a;

	return rgba(float4(mandelbrot_color*0.25+frame*4, mandelbrot_color+frame, mandelbrot_color, 1.0));
}
