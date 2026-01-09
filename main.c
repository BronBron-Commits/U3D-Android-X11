#include <X11/Xlib.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <unistd.h>
#include <math.h>
#include <stdbool.h>

#define WIDTH  480
#define HEIGHT 800

const char *vs_src =
"attribute vec3 aPos;\n"
"uniform mat4 uMVP;\n"
"void main() {\n"
"  gl_Position = uMVP * vec4(aPos, 1.0);\n"
"}\n";

const char *fs_src =
"precision mediump float;\n"
"void main() {\n"
"  gl_FragColor = vec4(0.2, 0.8, 1.0, 1.0);\n"
"}\n";

GLuint compile(GLenum t, const char *s) {
    GLuint sh = glCreateShader(t);
    glShaderSource(sh, 1, &s, NULL);
    glCompileShader(sh);
    return sh;
}

/* column-major math */

void mat4_identity(float *m) {
    for (int i = 0; i < 16; i++) m[i] = 0.0f;
    m[0]=m[5]=m[10]=m[15]=1.0f;
}

void mat4_translate(float *m, float z) {
    mat4_identity(m);
    m[14] = z;
}

void mat4_rotate_x(float *m, float a) {
    mat4_identity(m);
    m[5]  =  cosf(a);
    m[6]  =  sinf(a);
    m[9]  = -sinf(a);
    m[10] =  cosf(a);
}

void mat4_rotate_y(float *m, float a) {
    mat4_identity(m);
    m[0]  =  cosf(a);
    m[2]  = -sinf(a);
    m[8]  =  sinf(a);
    m[10] =  cosf(a);
}

void mat4_perspective(float *m, float fov, float asp, float n, float f) {
    float t = tanf(fov * 0.5f);
    for (int i = 0; i < 16; i++) m[i] = 0.0f;
    m[0]  = 1.0f / (asp * t);
    m[5]  = 1.0f / t;
    m[10] = -(f + n) / (f - n);
    m[11] = -1.0f;
    m[14] = -(2.0f * f * n) / (f - n);
}

void mat4_mul(float *o, float *a, float *b) {
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++)
            o[c*4+r] =
                a[0*4+r] * b[c*4+0] +
                a[1*4+r] * b[c*4+1] +
                a[2*4+r] * b[c*4+2] +
                a[3*4+r] * b[c*4+3];
}

int main() {
    Display *xd = XOpenDisplay(NULL);
    int scr = DefaultScreen(xd);

    Window win = XCreateSimpleWindow(
        xd, RootWindow(xd, scr),
        0, 0, WIDTH, HEIGHT, 0,
        BlackPixel(xd, scr),
        WhitePixel(xd, scr)
    );

    XSelectInput(xd, win,
        ButtonPressMask |
        ButtonReleaseMask |
        PointerMotionMask
    );

    XMapWindow(xd, win);

    EGLDisplay ed = eglGetDisplay((EGLNativeDisplayType)xd);
    eglInitialize(ed, NULL, NULL);

    EGLint cfg_attr[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_DEPTH_SIZE, 16,
        EGL_NONE
    };

    EGLConfig cfg;
    EGLint n;
    eglChooseConfig(ed, cfg_attr, &cfg, 1, &n);

    EGLSurface surf = eglCreateWindowSurface(ed, cfg, (EGLNativeWindowType)win, NULL);
    EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLContext ctx = eglCreateContext(ed, cfg, EGL_NO_CONTEXT, ctx_attr);
    eglMakeCurrent(ed, surf, surf, ctx);

    glViewport(0, 0, WIDTH, HEIGHT);
    glEnable(GL_DEPTH_TEST);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, compile(GL_VERTEX_SHADER, vs_src));
    glAttachShader(prog, compile(GL_FRAGMENT_SHADER, fs_src));
    glLinkProgram(prog);
    glUseProgram(prog);

    float verts[] = {
        -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,
         0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f,
        -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,
         0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f
    };

    unsigned short idx[] = {
        0,1,2, 2,3,0,
        4,5,6, 6,7,4,
        0,4,7, 7,3,0,
        1,5,6, 6,2,1,
        3,2,6, 6,7,3,
        0,1,5, 5,4,0
    };

    GLuint vbo, ibo;
    glGenBuffers(1,&vbo);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(verts),verts,GL_STATIC_DRAW);

    glGenBuffers(1,&ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(idx),idx,GL_STATIC_DRAW);

    GLint aPos = glGetAttribLocation(prog,"aPos");
    glEnableVertexAttribArray(aPos);
    glVertexAttribPointer(aPos,3,GL_FLOAT,GL_FALSE,0,0);

    GLint uMVP = glGetUniformLocation(prog,"uMVP");

    float proj[16], view[16], rx[16], ry[16], tmp[16], mvp[16];
    mat4_perspective(proj, 1.0f, (float)WIDTH/HEIGHT, 0.1f, 50.0f);
    mat4_translate(view, -4.0f);

    bool dragging = false;
    int last_x = 0, last_y = 0;
    float rot_x = 0.0f, rot_y = 0.0f;

    while (1) {
        while (XPending(xd)) {
            XEvent e;
            XNextEvent(xd, &e);

            if (e.type == ButtonPress) {
                dragging = true;
                last_x = e.xbutton.x;
                last_y = e.xbutton.y;
            }

            if (e.type == ButtonRelease) {
                dragging = false;
            }

            if (e.type == MotionNotify && dragging) {
                int dx = e.xmotion.x - last_x;
                int dy = e.xmotion.y - last_y;
                last_x = e.xmotion.x;
                last_y = e.xmotion.y;

                rot_y += dx * 0.01f;
                rot_x += dy * 0.01f;
            }
        }

        mat4_rotate_y(ry, rot_y);
        mat4_rotate_x(rx, rot_x);
        mat4_mul(tmp, ry, rx);
        mat4_mul(tmp, view, tmp);
        mat4_mul(mvp, proj, tmp);

        glClearColor(0.05f,0.05f,0.08f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glUniformMatrix4fv(uMVP,1,GL_FALSE,mvp);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);

        eglSwapBuffers(ed, surf);
        usleep(16000);
    }
}
