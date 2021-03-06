
//-------------------------------------------------------------------------------------------
//  University of Victoria Computer Science Department
//	FrameWork for OpenGL application under QT
//  Course title: Computer Graphics CSC305
//-------------------------------------------------------------------------------------------
#include <QtGui>
#include <QtOpenGL>
#include <QMessageBox>

#include <math.h>
#include <stddef.h>
#include <list>
#include <utility>
#include "glwidget.h"
#include "PS_FrameWork/include/PS_FileDirectory.h"
#include "PS_FrameWork/include/PS_AppConfig.h"
#include "PS_FrameWork/include/PS_ErrorManager.h"
#include "PS_FrameWork/include/PS_Property.h"
//#include "PS_SubDivision/include/CSubDivCCDX11.h"
#include "CEaseInEaseOut.h"
#include "PS_PerfTest.h"
#include "PS_BlobTree/include/BlobTreeBuilder.h"
#include "_GlobalSettings.h"
#include "CCubeTable.h"
#include "ConversionToActions.h"
#include "PS_HighPerformanceRender.h"

#include "CBlobTreeNetwork.h"
#include "CBlobTreeAnimation.h"
#include "GalinMedusaGenerator.h"
#include "SampleShapes.h"
#include "DlgPizaModel.h"
#include "GL/glu.h"

using namespace PS::FILESTRINGUTILS;
using namespace PS::BLOBTREEANIMATION;
using namespace Qt;

//FOVY
#define FIELD_OF_VIEW_Y  60.0
#define Z_NEAR 0.03f
#define Z_FAR  3000.0f

#define Left	-8.0
#define Right	+8.0
#define Bottom	-8.0
#define Top	+8.0
#define Near    -10.0
#define Far     +10.0
#define ANIMATION_FRAME_TIME 0.001f

GLfloat vertices [][3] = {{-1.0, -1.0, 1.0}, {-1.0, 1.0, 1.0}, {1.0, 1.0, 1.0},
                          {1.0, -1.0, 1.0}, {-1.0, -1.0, -1.0}, {-1.0, 1.0, -1.0},
                          {1.0, 1.0, -1.0}, {1.0, -1.0, -1.0}};



GLWidget::GLWidget(QWidget *parent)
    : QGLWidget(parent)
{    
    this->setMouseTracking(true);

    //Setup TBB Task Scheduling system
    tbb::task_scheduler_init init;
    TaskManager::getTaskManager()->init();


    //Reset
    resetTransformState();

    //DlgFieldEditor
    m_dlgFieldEditor = new DlgFieldFunctionEditor(this);
    connect(m_dlgFieldEditor, SIGNAL(sig_actExecuteCommand(int, QString)), this, SLOT(actNetRecvCommand(int, QString)));
    connect(m_dlgFieldEditor, SIGNAL(sig_actSetProgress(int, int, int)), this, SIGNAL(sig_setProgress(int, int, int)));

    m_layerManager.setActiveLayer(-1);

    m_animSpeed			= 1.0f;
    m_glChessBoard		= 0;
    m_uiMode			= uimSketch;
    m_sketchType                = bntPrimPoint;     
    m_bEnableCamLeftKey         = false;

    m_mouseButton		= CArcBallCamera::mbNone;
    m_modelBlobTree		= NULL;
    m_modelLayerManager         = NULL;
    m_modelBlobNodeProperty     = NULL;
    m_modelStats                = NULL;
    m_modelColorRibbon		= NULL;    
    m_lpUIWidget                = NULL;

    m_idxRibbonSelection = 0;
    m_bTransformSkeleton = false;
    m_bProbing = false;

    //Undo/Redo
    m_curUndoLevel = 0;
    m_ctUndoLevels = 0;
    m_lstUndo.resize(MAX_UNDO_LEVEL);

    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(advanceAnimation()));
}

GLWidget::~GLWidget()
{
    makeCurrent();

    //Cleanup Memory
    TaskManager::getTaskManager()->shutdown();
    m_layerManager.removeAllLayers();

    //m_parsip.removeAllMPUs();
    //m_optParsip.removeAllMPUs();

    //Clear Undo List
    resetUndoLevels();
    m_lstUndo.clear();

    SAFE_DELETE(m_dlgFieldEditor);
    SAFE_DELETE(m_timer);
    SAFE_DELETE(m_modelBlobTree);
    SAFE_DELETE(m_modelLayerManager);
    SAFE_DELETE(m_modelBlobNodeProperty);
    SAFE_DELETE(m_modelStats);
    SAFE_DELETE(m_modelColorRibbon);
    SAFE_DELETE(m_lpUIWidget);


    SAFE_DELETE(m_glShaderNormalMesh);
    SAFE_DELETE(m_glShaderSelection);

    //Delete newly created m_object
    glDeleteLists(m_glChessBoard, 1);
    glDeleteLists(m_glTopCornerCube, 1);
}

void GLWidget::initializeGL()
{
    //Background color will be gray
    if(TheAppSettings::Instance().setDisplay.bDarkBackground)
        glClearColor(0.45f, 0.45f, 0.45f, 1.0f);
    else
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    //Create ground
    /*
 glEnable(GL_TEXTURE_2D);
 GLubyte image[64][64][3];
 int i,j,c;

 for(i = 0; i < 64; i++)
 {
  for(j = 0; j < 64; j++)
  {
   c = (((i & 0x8) == 0)^((j & 0x8) == 0)) * 255;
   if(c == 0) c = 70;

   image[i][j][0] = (GLubyte)c;
   image[i][j][1] = (GLubyte)c;
   image[i][j][2] = (GLubyte)c;
  }
 }

 glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 64, 64, 0, GL_RGB, GL_UNSIGNED_BYTE, image);

 glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
 glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

 glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
 glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
 */
    //====================================================================
    //Setting up lights
    static const GLfloat lightColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    static const GLfloat lightPos[4] = { 5.0f, 5.0f, 10.0f, 0.0f };


    //Set Colors of Light
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightColor);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightColor);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightColor);

    //Set Light Position
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    //Turn on Light 0
    glEnable(GL_LIGHT0);

    //Enable Lighting
    glEnable(GL_LIGHTING);
    //Enable features we want to use from OpenGL
    glShadeModel(GL_SMOOTH);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_NORMALIZE);
    //============================================================================
    m_glChessBoard = glGenLists(1);
    glNewList(m_glChessBoard, GL_COMPILE);
    drawChessBoard();
    glEndList();

    m_glTopCornerCube = drawTopCornerCube();

    /*
 bool bGlewInit = m_shader.init();
 FlushAllErrors();

 //Load shader
 if(bGlewInit)
 {
  DAnsiStr strPath = ExtractFilePath(GetExePath());
  DAnsiStr vshader = strPath + "Resources\\Shaders\\TPhongVS.glsl";
  DAnsiStr fshader = strPath + "Resources\\Shaders\\TPhongFS.glsl";
  int res = m_shader.compile(vshader.ptr(), fshader.ptr());
  if(res == GL_SUCCESS)
  {
   //m_shader.run();
   //m_uniformTime = m_shader.getUniformLocation("time");
  }
  FlushAllErrors();
 }
 */

    //Shading using programmable stages of the pipeline
    DAnsiStr strPath = ExtractFilePath(GetExePath());
    DAnsiStr strVShaderPath = strPath + "Resources//Shaders//TPhongVS.glsl";
    DAnsiStr strFShaderPath = strPath + "Resources//Shaders//TPhongFS.glsl";

    m_glShaderNormalMesh = new QGLShaderProgram(this->context(), this);
    m_glShaderNormalMesh->addShaderFromSourceFile(QGLShader::Vertex, QString(strVShaderPath.ptr()));
    m_glShaderNormalMesh->addShaderFromSourceFile(QGLShader::Fragment, QString(strFShaderPath.ptr()));
    qDebug() << m_glShaderNormalMesh->log();
    if(!m_glShaderNormalMesh->link())
    {
        QString strError = "Unable to link shader file. " + m_glShaderNormalMesh->log();
        ReportError(strError.toLatin1());
        FlushAllErrors();
    }

    //Shader selected mesh
    strVShaderPath = strPath + "Resources//Shaders//TPhongSelectionVS.glsl";
    strFShaderPath = strPath + "Resources//Shaders//TPhongSelectionFS.glsl";

    m_glShaderSelection = new QGLShaderProgram(this->context(), this);
    m_glShaderSelection->addShaderFromSourceFile(QGLShader::Vertex, QString(strVShaderPath.ptr()));
    m_glShaderSelection->addShaderFromSourceFile(QGLShader::Fragment, QString(strFShaderPath.ptr()));
    qDebug() << m_glShaderSelection->log();
    if(!m_glShaderSelection->link())
    {
        QString strError = "Unable to link shader file. " + m_glShaderSelection->log();
        ReportError(strError.toLatin1());
        FlushAllErrors();
    }

    m_glShaderSelTime = m_glShaderSelection->uniformLocation("time");

}

void GLWidget::paintGL()
{
    //Clear target buffer and depth buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    //Pan Camera
    glTranslatef(m_globalPan.x, m_globalPan.y, m_globalPan.z);

    vec3f p = m_camera.getCoordinates();
    vec3f c = m_camera.getCenter();
    gluLookAt(p.x, p.y, p.z, c.x, c.y, c.z, 0.0, 1.0, 0.0);


    //Draw Layer Manager
    drawLayerManager();

    //Draw Ground
    if(TheAppSettings::Instance().setDisplay.bDrawChessboardGround)
    {
        glCallList(m_glChessBoard);
    }

    if(m_bProbing)
    {
        glPushMatrix();
        glTranslatef(m_probePoint.x, m_probePoint.y, m_probePoint.z);
        glScalef(0.1f, 0.1f, 0.1f);
        glCallList(m_glTopCornerCube);
        glPopMatrix();

        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glPointSize(3.0f);
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_POINTS);
        glVertex3fv(m_probeProjected.ptr());
        glEnd();

        glLineWidth(2.0f);
        glBegin(GL_LINES);
        glVertex3fv(m_probePoint.ptr());
        glVertex3fv(m_probeProjected.ptr());
        glEnd();
        glPopAttrib();
    }


    //Draw based on current UI mode
    U32 ctAnimObjects = CAnimManagerSingleton::Instance().countObjects();
    if((TheAppSettings::Instance().setDisplay.bShowAnimCurves)&&(ctAnimObjects > 0))
    {
        for(U32 i=0; i<ctAnimObjects; i++)
        {
            CAnimManagerSingleton::Instance().getObject(i)->drawPathCtrlPoints();
            CAnimManagerSingleton::Instance().getObject(i)->drawPathCurve();
        }
    }

    if(m_uiMode == uimSketch)
        drawPolygon(m_lstSketchControlPoints);
    else if(m_uiMode == uimTransform)
    {
        glDisable(GL_DEPTH_TEST);
        drawLineSegment(TheUITransform::Instance().mouseDown,
                        TheUITransform::Instance().mouseDown +
                        TheUITransform::Instance().mouseMove, vec4f(0,0,0,1));

        if(m_bProbing)
        {
            c = m_probePoint;
        }
        else
        {
            if(m_layerManager.hasActiveSelOctree())
                c = m_layerManager.getActiveSelOctree().center();
            else if(CAnimManagerSingleton::Instance().hasSelectedCtrlPoint())
            {
                CAnimObject* obj = CAnimManagerSingleton::Instance().getActiveObject();
                c = obj->path->getControlPoints()[obj->idxSelCtrlPoint];
            }
            else
                c = TheUITransform::Instance().mouseDown;
        }


        if(m_lpUIWidget)
        {
            m_lpUIWidget->setPos(c);
            m_lpUIWidget->draw();
        }
        glEnable(GL_DEPTH_TEST);
    }

    if(TheAppSettings::Instance().setDisplay.bShowGraph)
    {
        drawGraph();
    }


    //if(m_uiMode == uimSelect)
    //	drawRay(m_uiSelectRay, vec4f(0.0f, 1.0f, 0.0f, 1.0f));
}

void GLWidget::resizeGL(int width, int height)
{
    m_scrDim = vec2i(width, height);
    glViewport (0, 0, (GLsizei) width, (GLsizei) height);

    //Set projection matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(FIELD_OF_VIEW_Y,(GLdouble) width/height, Z_NEAR, Z_FAR);

    //Return to ModelView Matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void GLWidget::keyPressEvent( QKeyEvent *event )
{
    switch(event->key())
    {
    //Translate
    case(Key_G):
        actEditTranslate();
        break;

        //Scale
    case(Key_S):
        actEditScale();
        break;

        //Rotate
    case(Key_R):
        actEditRotate();
        break;

    case(Key_X):
        actEditAxisX();
        break;
    case(Key_Y):
        actEditAxisY();
        break;
    case(Key_Z):
        actEditAxisZ();
        break;
    }

}



void GLWidget::mouseDoubleClickEvent( QMouseEvent * event )
{
    setPrimitiveColorFromColorDlg();
}

bool GLWidget::queryScreenToRay( int x, int y, CRay& outRay )
{
    vec3f posNear(x, m_scrDim.y - y, 0.0f);
    vec3f posFar(x, m_scrDim.y - y, 1.0f);
    vec3f posTransNear, posTransFar;
    vec3f dir;

    if(windowToObject(posNear, posTransNear) && windowToObject(posFar, posTransFar))
    {
        dir = posTransFar - posTransNear;
        dir.normalize();


        outRay.set(posTransNear, dir);
        return true;
    }
    else
    {
        outRay.set(posTransNear, posTransFar - posTransNear);
    }

    return false;
}



void GLWidget::mousePressEvent(QMouseEvent *event)
{
    int x = event->x();
    int y = event->y();

    int idxLayer, idxQuery = -1;
    vec3f posNear(x, m_scrDim.y - y, 0.0f);
    vec3f posFar(x, m_scrDim.y - y, 1.0f);
    vec3f posTransNear;
    vec3f posTransFar;
    CRay  ray;

    if(windowToObject(posNear, posTransNear) && windowToObject(posFar, posTransFar))
    {
        vec3f dir = posTransFar - posTransNear;
        dir.normalize();
        ray.set(posTransNear, dir);
    }

    //Use LeftButton
    if(event->buttons() & Qt::LeftButton)
    {
        if(m_bEnableCamLeftKey)
            m_uiMode = uimSelect;
        m_mouseButton = PS::CArcBallCamera::mbLeft;
        if(m_uiMode == uimTransform)
            TheUITransform::Instance().mouseDown = posTransNear;
        else if((m_uiMode == uimSketch)||(m_uiMode == uimAnimation))
        {
            vec3f c(0.0f, 0.0f, 0.0f);

            if(m_layerManager.queryHitOctree(ray, Z_NEAR, Z_FAR, idxLayer, idxQuery))
                c = m_layerManager[idxLayer]->queryGetItem(idxQuery)->getOctree().center();

            vec3f ptInSpace = posTransNear + posTransNear.distance(c) * ray.direction;
            if(m_uiMode == uimSketch)
                m_lstSketchControlPoints.push_back(ptInSpace);
            else if(m_uiMode == uimAnimation)
            {
                //Draw animation curve
                CLayer* active = m_layerManager.getActiveLayer();
                if(active)
                {
                    CAnimObject* obj = CAnimManagerSingleton::Instance().getObject(active->selGetItem(0));
                    if(obj)
                    {
                        obj->path->addPoint(ptInSpace);
                        obj->path->populateTableAdaptive();
                    }
                }
            }
            updateGL();
        }
        else
            m_uiMode = uimSketch;
    }
    else if(event->buttons() & Qt::MidButton)
    {
        m_mouseButton = PS::CArcBallCamera::mbMiddle;
    }
    else if(event->buttons() & Qt::RightButton)
    {
        m_mouseButton = PS::CArcBallCamera::mbRight;

        //Select Operators with shift
        CLayer* lpActive = m_layerManager.getActiveLayer();
        if(lpActive)
        {
            if(QApplication::keyboardModifiers() == Qt::ShiftModifier)
                lpActive->queryBlobTree(false, true);
            else
                lpActive->queryBlobTree(true, false);
        }

        if(m_layerManager.queryHitOctree(ray, Z_NEAR, Z_FAR, idxLayer, idxQuery))
        {
            bool bMultiSelect = false;
            if(QApplication::keyboardModifiers() == Qt::ControlModifier)
                bMultiSelect = true;

            //Select the layer that we hit one of its primitives
            selectLayer(idxLayer);

            //Select the primitive and show its properties
            selectBlobNode(idxLayer,
                           m_layerManager[idxLayer]->queryGetItem(idxQuery),
                           bMultiSelect);
        }
        else
        {
            //No BlobTree primitive selected.Remove all selected nodes
            m_layerManager.selRemoveItems();

            //Check if we can select a ctrl point in path animation
            int idxPath, idxCtrlPoint;
            if(CAnimManagerSingleton::Instance().queryHitPathCtrlPoint(ray, Z_NEAR, Z_FAR, idxPath, idxCtrlPoint))
            {
                CAnimManagerSingleton::Instance().getObject(idxPath)->idxSelCtrlPoint = idxCtrlPoint;
                CAnimManagerSingleton::Instance().setActiveObject(idxPath);

                actEditTranslate();
            }
            else
            {
                CAnimManagerSingleton::Instance().queryHitResetAll();
                m_uiMode = uimSelect;
            }
        }
    }

    //Update
    updateGL();
}

void GLWidget::mouseMoveEvent(QMouseEvent *event)
{
    int x = event->x();
    int y = event->y();

    int dx = x - m_mouseLastPos.x;
    int dy = m_mouseLastPos.y - y;

    if(m_uiMode == uimTransform)
    {
        if((m_mouseButton == CArcBallCamera::mbNone) && m_lpUIWidget)
        {
            vec3f posNear(x, m_scrDim.y - y, 0.0f);
            vec3f posFar(x, m_scrDim.y - y, 1.0f);
            vec3f posTransNear;
            vec3f posTransFar;
            CRay  ray;

            if(windowToObject(posNear, posTransNear) && windowToObject(posFar, posTransFar))
            {
                vec3f dir = posTransFar - posTransNear;
                dir.normalize();
                ray.set(posTransNear, dir);
            }

            if(m_lpUIWidget->selectAxis(ray, Z_NEAR, Z_FAR) != uiaFree)
                updateGL();
        }
        else if(m_mouseButton == CArcBallCamera::mbLeft)
        {
            if(windowToObject(vec3f(x, m_scrDim.y - y, 0.0f), TheUITransform::Instance().mouseMove))
            {
                //Capture undo level for the first step in tranform
                if(TheUITransform::Instance().nStep == 0)
                    addUndoLevel();
                TheUITransform::Instance().nStep++;

                vec3f displacement = mask(TheUITransform::Instance().mouseMove - TheUITransform::Instance().mouseDown, TheUITransform::Instance().axis);
                displacement *= static_cast<float>(TheAppSettings::Instance().setSketch.mouseDragScale);
                TheUITransform::Instance().mouseDown = TheUITransform::Instance().mouseMove;

                if(TheUITransform::Instance().type == uitTranslate)
                    TheUITransform::Instance().translate = displacement;
                else if(TheUITransform::Instance().type == uitScale)
                    TheUITransform::Instance().scale = displacement;
                else if(TheUITransform::Instance().type == uitRotate)
                {
                    vec3f axis;
                    float angle;

                    //Setting ergonomic displacements for affine rotation
                    switch(TheUITransform::Instance().axis)
                    {
                    case(uiaX):
                        axis = vec3f(1.0f, 0.0f, 0.0f);
                        angle = displacement.x;
                        break;
                    case(uiaY):
                        axis = vec3f(0.0f, 1.0f, 0.0f);
                        angle = displacement.y;
                        break;
                    case(uiaZ):
                        axis = vec3f(0.0f, 0.0f, 1.0f);
                        angle = displacement.z;
                        break;
                    default:
                        axis = vec3f(1.0f, 1.0f, 1.0f);
                        angle = displacement.x;
                    }

                    TheUITransform::Instance().rotate.fromAngleAxis(angle, axis);
                }

                //Probing
                if(m_bProbing)
                {
                    m_probePoint += TheUITransform::Instance().translate;
                    updateProbe();
                    return;
                }

                //Now apply tranform to the selected object
                CLayer* active = m_layerManager.getActiveLayer();
                size_t ctSelected = active->selCountItems();
                if((ctSelected > 0) && (displacement.isZero() == false))
                {
                    active->getTracker().bump();
                    for(size_t i=0; i<ctSelected; i++)
                    {
                        CBlobNode* node = active->selGetItem(i);
                        if(node && node->getLock().acquire())
                        {
                            //actNetSendCommand(cmdLock, node, vec4f(0.0f, 0.0f, 0.0f, 0.0f));

                            //Increment version for polygonizer
                            //COctree selOct = node->getOctree();

                            //Apply affine transform to internal affine node inside each skeletal primitive
                            if(TheUITransform::Instance().type == uitTranslate)
                            {
                                if(m_bTransformSkeleton && (node->getNodeType() == bntPrimSkeleton))
                                {
                                    CSkeletonPrimitive* sprim = reinterpret_cast<CSkeletonPrimitive*>(node);
                                    sprim->getSkeleton()->translate(TheUITransform::Instance().translate);
                                }
                                else
                                    node->getTransform().addTranslate(TheUITransform::Instance().translate);
                                //selOct.translate(TheUITransform::Instance().translate);

                                //Send Message
                                actNetSendCommand(cmdMove, node, vec4f(TheUITransform::Instance().translate, 0.0f));
                            }
                            else if(TheUITransform::Instance().type == uitScale)
                            {
                                node->getTransform().addScale(TheUITransform::Instance().scale);
                                //selOct.scale(TheUITransform::Instance().scale);
                                //selOct.scale(node->getTransform().getScale());

                                //Send Message
                                actNetSendCommand(cmdScale, node, vec4f(TheUITransform::Instance().scale, 1.0f));
                            }
                            else if(TheUITransform::Instance().type == uitRotate)
                            {
                                node->getTransform().addRotate(TheUITransform::Instance().rotate);
                                //selOct.rotate(TheUITransform::Instance().rotate);

                                //Send Message
                                actNetSendCommand(cmdRotate, node, TheUITransform::Instance().rotate.getAsVec4f());
                            }

                            node->getLock().release();
                        }
                    }

                    //Repolygonize and update Screen
                    actMeshPolygonize(m_layerManager.getActiveLayerIndex());

                }//IF BlobNode Selected
                else
                {
                    CAnimObject* obj = CAnimManagerSingleton::Instance().getActiveObject();

                    if(obj)
                    {
                        std::vector<vec3f> lstCtrlPoints = obj->path->getControlPoints();
                        if(obj->path->isCtrlPointIndex(obj->idxSelCtrlPoint))
                        {
                            vec3f c = lstCtrlPoints[obj->idxSelCtrlPoint] + TheUITransform::Instance().translate;
                            obj->path->setPoint(obj->idxSelCtrlPoint, c);
                            obj->path->populateTableAdaptive();
                            updateGL();
                        }
                    }
                }

            }
        }
    }//Transform

    //Control Camera in Select Mode
    //Or Left Button on laptop
    if((m_mouseButton == PS::CArcBallCamera::mbMiddle)||
       (m_bEnableCamLeftKey && (m_mouseButton == PS::CArcBallCamera::mbLeft)))
    {
        if(QApplication::keyboardModifiers() == Qt::ControlModifier)
        {
            //m_camera.setCenter(m_camera.getCenter() + vec3f(0.03f * dx, 0.03f * dy, 0.0f));
            m_globalPan.x += MOUSE_MOVE_COEFF * dx;
            m_globalPan.y += MOUSE_MOVE_COEFF * dy;
            m_globalPan.z = 0.0f;
        }
        else
        {
            m_camera.setHorizontalAngle(m_camera.getHorizontalAngle() + (MOUSE_MOVE_COEFF * dx));
            m_camera.setVerticalAngle(m_camera.getVerticalAngle() + (MOUSE_MOVE_COEFF * dy));
        }
        updateGL();
    }


    m_mouseLastPos = vec2i(x, y);
}


void GLWidget::mouseReleaseEvent(QMouseEvent *event )
{
    if(m_layerManager.getActiveLayer() == NULL)
    {
        m_mouseButton = CArcBallCamera::mbNone;
        return;
    }

    if(m_mouseButton == CArcBallCamera::mbLeft)
    {
        if(m_uiMode == uimTransform)
        {
            //First reset to avoid the glitch after transformation
            resetTransformState();
        }
        else if(m_uiMode == uimSketch)
        {
            if(m_lstSketchControlPoints.size() > 0)
            {
                addBlobPrimitive(m_sketchType, m_lstSketchControlPoints.back());
                m_lstSketchControlPoints.clear();
            }
        }
    }

    m_mouseButton = CArcBallCamera::mbNone;

    updateGL();
}

void GLWidget::wheelEvent(QWheelEvent *event)
{
    m_camera.setZoom(m_camera.getCurrentZoom() - MOUSE_WHEEL_COEFF * event->delta());
    emit sig_viewZoomChanged(m_camera.getCurrentZoom());
    updateGL();
}

void GLWidget::drawCubePolygonTextured(int a, int b, int c, int d)
{
    glBegin(GL_POLYGON);
    glTexCoord2f(0.0, 0.0);
    glVertex3fv(vertices[a]);

    glTexCoord2f(0.0, 1.0);
    glVertex3fv(vertices[b]);


    glTexCoord2f(1.0, 1.0);
    glVertex3fv(vertices[c]);

    glTexCoord2f(1.0, 0.0);
    glVertex3fv(vertices[d]);
    glEnd();
}

void GLWidget::drawCubePolygon(int a, int b, int c, int d)
{
    glBegin(GL_POLYGON);
    glVertex3fv(vertices[a]);
    glVertex3fv(vertices[b]);
    glVertex3fv(vertices[c]);
    glVertex3fv(vertices[d]);
    glEnd();
}

void GLWidget::drawChessBoard()
{
    int rows = 50;
    int cols = 50;

    float halfRows = (float)rows / 2.0f;
    float halfCols = (float)cols / 2.0f;

    glPushMatrix();
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    //glDisable(GL_LIGHTING);

    glTranslated(-halfRows, 0.0, -halfCols);

    //CMeshVV::setOglMaterial(CMaterial::mtrlBlack());
    //glShadeModel(GL_SMOOTH);
    glColor3f(0.0f, 0.0f, 0.0f);

    //Draw Grid
    glBegin(GL_LINES);
    //Horizontal lines.
    for (int i=0; i<=rows; i++)
    {
        glVertex3f(0, 0, i);
        glVertex3f(cols, 0, i);
    }
    //Vertical lines.
    for (int i=0; i<=cols; i++)
    {
        glVertex3f(i, 0, 0);
        glVertex3f(i, 0, rows);
    }
    glEnd();

    //Draw Axes
    glLineWidth(2.0f);
    glColor3f(1.0f, 0.0f, 0.0f);
    glBegin(GL_LINES);
    glVertex3f(0, 0, halfRows);
    glVertex3f(cols, 0, halfRows);
    glEnd();

    glColor3f(0.0f, 0.0f, 1.0f);
    glBegin(GL_LINES);
    glVertex3f(halfCols, 0, 0);
    glVertex3f(halfCols, 0, rows);
    glEnd();


    //glEnable(GL_LIGHTING);
    glPopAttrib();
    glPopMatrix();
}

GLuint GLWidget::drawDefaultColoredCube()
{
    //Material Colors for 3 pairs of surfaces. Each pair are parallel surfaces with the same color
    static const GLfloat colorRed[4] = { 0.8f, 0.1f, 0.0f, 1.0f }; //Red
    static const GLfloat colorGreen[4] = { 0.0f, 0.8f, 0.2f, 1.0f }; //Green
    static const GLfloat colorBlue[4] = { 0.2f, 0.2f, 1.0f, 1.0f }; //Blue

    //Creating and compiling a list of objects
    GLuint list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glShadeModel(GL_SMOOTH);

    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, colorRed);
    drawCubePolygon(0, 3, 2, 1);
    drawCubePolygon(4, 5, 6, 7);


    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, colorGreen);
    drawCubePolygon(3, 0, 4, 7);
    drawCubePolygon(1, 2, 6, 5);


    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, colorBlue);
    drawCubePolygon(2, 3, 7, 6);
    drawCubePolygon(5, 4, 0, 1);

    glEndList();

    return list;
}

GLuint GLWidget::drawTopCornerCube()
{
    static const GLfloat colorRed[4] = { 0.8f, 0.1f, 0.0f, 1.0f }; //Red
    static const GLfloat colorGreen[4] = { 0.0f, 0.8f, 0.2f, 1.0f }; //Green
    static const GLfloat colorBlue[4] = { 0.2f, 0.2f, 1.0f, 1.0f }; //Blue

    //Creating and compiling a list of objects
    GLuint list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glShadeModel(GL_SMOOTH);

    //glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, colorRed);
    glColor4fv(colorRed);
    drawCubePolygon(2, 3, 7, 6);
    drawCubePolygon(5, 4, 0, 1);

    //glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, colorGray);
    glColor4fv(colorGreen);
    drawCubePolygon(3, 0, 4, 7);
    drawCubePolygon(1, 2, 6, 5);
    //font.Render("Left", -1, FTPoint(-1, 1));

    //glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, colorGray);
    glColor4fv(colorBlue);
    drawCubePolygon(0, 3, 2, 1);
    drawCubePolygon(4, 5, 6, 7);


    glEndList();

    return list;
}

void GLWidget::setParsipCellSize(float value)
{
    if(TheAppSettings::Instance().setParsip.cellSize != value)
    {
        TheAppSettings::Instance().setParsip.cellSize = value;
        m_layerManager.bumpRevisions();
    }
}

void GLWidget::setParsipGridDim( int nComboItem )
{
    if(nComboItem >=0 && nComboItem < 3)
    {
        TheAppSettings::Instance().setParsip.griddim = (1 << (nComboItem + 3));
        m_layerManager.bumpRevisions();
    }
}

void GLWidget::setParsipNormalsGeodesicAngle( float value )
{
    if(TheAppSettings::Instance().setParsip.adaptiveParam != value)
    {
        TheAppSettings::Instance().setParsip.adaptiveParam = value;
        m_layerManager.bumpRevisions();
    }
}

void GLWidget::setParsipThreadsCount(int value)
{
    TaskManager::getTaskManager()->shutdown();
    TaskManager::getTaskManager()->init(value);
    TheAppSettings::Instance().setParsip.ctThreads = TaskManager::getTaskManager()->getThreadCount();
}

void GLWidget::setParsipUseTBB(bool bEnable)
{
    TheAppSettings::Instance().setParsip.bUseTBB = bEnable;
}

void GLWidget::setParsipUseComputeShaders(bool bEnable)
{
    TheAppSettings::Instance().setParsip.bUseComputeShaders = bEnable;
}

void GLWidget::setParsipUseAdaptiveSubDivision( bool bEnable )
{
    TheAppSettings::Instance().setParsip.bUseAdaptiveSubDivision = bEnable;
    m_layerManager.bumpRevisions();
}

void GLWidget::setParsipCellShapeTetraHedra(bool bTetra)
{
    if(bTetra) TheAppSettings::Instance().setParsip.cellShape = csTetrahedra;
    m_layerManager.bumpRevisions();
}

void GLWidget::setParsipCellShapeCube(bool bCube)
{
    if(bCube) TheAppSettings::Instance().setParsip.cellShape = csCube;
    m_layerManager.bumpRevisions();
}

void GLWidget::setDisplayMeshNone(bool bEnable)
{
    if(bEnable)
        TheAppSettings::Instance().setDisplay.showMesh = smNone;
    updateGL();
}

void GLWidget::setDisplayMeshWireFrame(bool bEnable)
{
    if(bEnable)
        TheAppSettings::Instance().setDisplay.showMesh = smWireFrame;
    updateGL();
}

void GLWidget::setDisplayMeshSurface(bool bEnable)
{
    if(bEnable)
        TheAppSettings::Instance().setDisplay.showMesh = smSurface;
    updateGL();
}

void GLWidget::setDisplayBoxLayer(bool bEnable)
{
    TheAppSettings::Instance().setDisplay.bShowBoxLayer = bEnable;
    updateGL();
}

void GLWidget::setDisplayBoxPrimitive(bool bEnable)
{
    TheAppSettings::Instance().setDisplay.bShowBoxPrimitive = bEnable;
    updateGL();
}

void GLWidget::setDisplayBoxPoly(bool bEnable)
{
    TheAppSettings::Instance().setDisplay.bShowBoxPoly = bEnable;
    updateGL();
}

void GLWidget::setDisplaySeedPoints(bool bEnable)
{
    TheAppSettings::Instance().setDisplay.bShowSeedPoints = bEnable;
    updateGL();
}

void GLWidget::setDisplayNormalsLength(int length)
{
    TheAppSettings::Instance().setDisplay.normalLength = length;
    updateGL();
}

void GLWidget::setDisplayNormals(bool bEnable)
{
    TheAppSettings::Instance().setDisplay.bShowNormals = bEnable;
    updateGL();
}

void GLWidget::setDisplayChessBoard(bool bEnable)
{
    TheAppSettings::Instance().setDisplay.bDrawChessboardGround = bEnable;
    updateGL();
}

void GLWidget::setDisplayMtrlMeshWires(int index)
{
    TheAppSettings::Instance().setDisplay.mtrlMeshWires = CMaterial::getMaterialFromList(index);
    updateGL();
}

void GLWidget::drawOctree(vec3f lo, vec3f hi, vec4f color, bool bSelected)
{
    float l = lo.x; float r = hi.x;
    float b = lo.y; float t = hi.y;
    float n = lo.z; float f = hi.z;
    //float lrov2 = (l+r)/2.0f;
    //float btov2 = (b+t)/2.0f;
    //float nfov2 = (n+f)/2.0f;

    GLfloat vertices [][3] = {{l, b, f}, {l, t, f}, {r, t, f},
                              {r, b, f}, {l, b, n}, {l, t, n},
                              {r, t, n}, {r, b, n}};

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    if(bSelected)
        glLineWidth(3.0f);
    else
        glLineWidth(1.0f);
    //glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color.ptr());
    glColor3fv(color.ptr());
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBegin(GL_QUADS);
    glVertex3fv(vertices[0]); glVertex3fv(vertices[3]); glVertex3fv(vertices[2]); glVertex3fv(vertices[1]);
    glVertex3fv(vertices[4]); glVertex3fv(vertices[5]); glVertex3fv(vertices[6]); glVertex3fv(vertices[7]);
    glVertex3fv(vertices[3]); glVertex3fv(vertices[0]); glVertex3fv(vertices[4]); glVertex3fv(vertices[7]);
    glVertex3fv(vertices[1]); glVertex3fv(vertices[2]); glVertex3fv(vertices[6]); glVertex3fv(vertices[5]);
    glVertex3fv(vertices[2]); glVertex3fv(vertices[3]); glVertex3fv(vertices[7]); glVertex3fv(vertices[6]);
    glVertex3fv(vertices[5]); glVertex3fv(vertices[4]); glVertex3fv(vertices[0]); glVertex3fv(vertices[1]);
    glEnd();
    glPopAttrib();
}

void GLWidget::drawLayerManager()
{
    //glDisable(GL_LIGHTING);
    //CMeshVV* aMesh = NULL;
    vector<vec3f> lstSeeds;
    size_t ctLayers = m_layerManager.countLayers();
    bool bIsActiveLayer = false;
    vec4f clBlack(0.0f, 0.0f, 0.0f, 1.0f);
    vec4f clGray(0.3f, 0.3f, 0.3f, 1.0f);
    vec4f clBlue(0.0f, 0.0f, 1.0f, 1.0f);
    vec4f clRed(1.0f, 0.0f, 0.0f, 1.0f);
    vec4f clOrange(1.0f, 0.5f, 0.0f, 1.0f);

    for(size_t iLayer=0; iLayer < ctLayers; iLayer++)
    {
        //if(m_layerManager[iLayer]->hasMesh() && m_layerManager[iLayer]->isVisible())
        if(m_layerManager[iLayer]->isVisible())
        {
            bIsActiveLayer = (m_layerManager.getActiveLayerIndex() == iLayer);
            //Draw Bounding Boxes
            CLayer* alayer = m_layerManager.getLayer(iLayer);

            //Bind shader for drawing mesh only
            m_glShaderNormalMesh->bind();
            glEnable(GL_LIGHTING);

           // alayer->getSimdPoly().draw(false);

            //Draw mesh in WireFrame Mode
            if(TheAppSettings::Instance().setDisplay.showMesh == smWireFrame)
            {
                glPushAttrib(GL_ALL_ATTRIB_BITS);
                //Draw WireFrame Mesh
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                if(alayer->hasMesh())
                    alayer->getMesh()->drawBuffered();
                else
                    alayer->getPolygonizer()->drawMesh();
                glPopAttrib();
            }
            else if(TheAppSettings::Instance().setDisplay.showMesh == smSurface)
            {
                glPushMatrix();
                glShadeModel(GL_SMOOTH);
                //Draw Surface Mesh
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                if(alayer->hasMesh())
                    alayer->getMesh()->drawBuffered();
                else
                    alayer->getPolygonizer()->drawMesh();
                glPopMatrix();
            }

            //Release shader program
            glDisable(GL_LIGHTING);
            m_glShaderNormalMesh->release();

            //Draw SeedPoints
            if(TheAppSettings::Instance().setDisplay.bShowSeedPoints)
            {
                //CMeshVV::setOglMaterial(CMaterial::mtrlGreen());
                glColor3f(0.0f, 1.0f, 0.0f);
                glPointSize(5.0f);
                glBegin(GL_POINTS);
                size_t ctSeeds = m_layerManager[iLayer]->getAllSeeds(lstSeeds);
                for(size_t i=0; i<ctSeeds; i++)
                    glVertex3fv(lstSeeds[i].ptr());
                glEnd();
            }

            //Draw Normals
            if(TheAppSettings::Instance().setDisplay.bShowNormals)
            {
                //m_parsip.drawNormals(iLayer, TheAppSettings::Instance().setDisplay.normalLength);
                //m_optParsip.drawNormals(TheAppSettings::Instance().setDisplay.normalLength);
                if(alayer->getMesh())
                    alayer->getMesh()->drawNormals(TheAppSettings::Instance().setDisplay.normalLength);
                else
                    alayer->getPolygonizer()->drawNormals(TheAppSettings::Instance().setDisplay.normalLength);
            }


            //Draw Layer Octree
            if(TheAppSettings::Instance().setDisplay.bShowBoxLayer)
            {
                COctree octree = alayer->getOctree();
                drawOctree(octree.lower, octree.upper, clOrange, true);
            }

            //Draw Primitive Octrees
            if(TheAppSettings::Instance().setDisplay.bShowBoxPrimitive)
            {
                size_t ctItems = alayer->queryCountItems();
                if(ctItems > 0)
                {
                    glPushAttrib(GL_ALL_ATTRIB_BITS);
                    //glLineStipple(3, 0xaa55);
                    for (size_t i=0; i<ctItems; i++)
                    {
                        if(alayer->queryGetItem(i)->isOperator())
                            continue;                        
                        COctree primOctree = alayer->queryGetItem(i)->getOctree();
                        drawOctree(primOctree.lower, primOctree.upper, clBlue, false);

                        //Draw Instance Transformed Point
                        if(alayer->queryGetItem(i)->getNodeType() == bntPrimInstance)
                        {
                            CInstance* lpInst = reinterpret_cast<CInstance*>(alayer->queryGetItem(i));
                            vec3f p = lpInst->getOctree().center();
                            vec3f side = vec3f(0.3, 0.3, 0.3);

                            //Transform to O(0,0,0)
                            p = lpInst->getTransform().applyBackwardTransform(p);
                            drawOctree(p - side, p + side, clBlack);

                            //Transform to base
                            p = lpInst->getOriginalNode()->getTransform().applyForwardTransform(p);
                            drawOctree(p - side, p + side, clOrange);

                        }
                    }
                    glPopAttrib();
                }
            }

            //Draw Octree for all selected items
            if(alayer->selCountItems() > 0)
            {
                glPushAttrib(GL_ALL_ATTRIB_BITS);
                //glLineStipple(3, 0xaa55);
                for(size_t i=0; i<alayer->selCountItems(); i++)
                {
                    COctree oct = alayer->selGetItem(i)->getOctree();
                    drawOctree(oct.lower, oct.upper, clRed, true);

                    //Draw Selection Mesh
                    /*
     float t = static_cast<float>(CPerfLogger::convertTimeTicksToMS(CPerfLogger::getPerfCounter()));
     m_glShaderSelection->bind();

     m_glShaderSelection->setUniformValue(m_glShaderSelTime, t);
     glPushAttrib(GL_ALL_ATTRIB_BITS);
      glColor3f(0.8f, 0.8, 0.8f);
      alayer->selGetMesh(i)->drawBuffered();
     glPopAttrib();
     m_glShaderSelection->release();
     */
                }
                glPopMatrix();
            }


            //Draw Polygonization Boxes
            if(TheAppSettings::Instance().setDisplay.bShowBoxPoly)
            {
                vec3f lo, hi;
                //size_t ctMPUs = m_parsip.countMPUs();
                size_t ctMPUs = alayer->getPolygonizer()->countMPUs();
                for (size_t iMPU=0; iMPU < ctMPUs; iMPU++)
                {
                    //if(m_parsip.getMPUExtent(iMPU, lo, hi))
                    if(alayer->getPolygonizer()->getMPUExtent(iMPU, lo, hi))
                    {
                        drawOctree(lo, hi, clBlack, false);
                    }
                }
            }
        }
    }
}

void GLWidget::actFileOpen()
{
    QString strFileName = QFileDialog::getOpenFileName(this, tr("Open Scene"),
                                                       GetExePath().ptr(),
                                                       tr("Blobtree Scene(*.scene);;SketchNET Actions(*.acts);;Obj Mesh(*.obj);;Parsip Binary Mesh File(*.msh)"));
    actFileOpen(strFileName);
}

void GLWidget::actFileOpen(QString strFile)
{
    DAnsiStr strFilePath(strFile.toLatin1().data());
    DAnsiStr strFileTitle	 = PS::FILESTRINGUTILS::ExtractFileName(strFilePath);
    DAnsiStr strFileExt		 = PS::FILESTRINGUTILS::ExtractFileExt(strFilePath);
    //TheAppSettings::Instance().setParsip.strLastScene = PS::FILESTRINGUTILS::ExtractFilePath(strFilePath);

    //m_optParsip.removeAllMPUs();
    if(strFileExt.toUpper() == "SCENE")
    {
        if(m_layerManager.loadScript(strFilePath))
        {
            actMeshPolygonize();
            emit sig_showLayerManager(getModelLayerManager());
            emit sig_showBlobTree(getModelBlobTree(0));
        }
    }
    else if(strFileExt.toUpper() == "ACTS")
    {
        m_layerManager.removeAllLayers();
        m_layerManager.addLayer(NULL, NULL, strFileTitle.ptr());
        m_layerManager.setActiveLayer(0);


        int index, isPending;
        CDesignNet::GetDesignNet()->addMember("127.0.0.1");

        CMember* vpeer = NULL;

        vpeer = CDesignNet::GetDesignNet()->findMember(QString("127.0.0.1"), &index, &isPending);
        if(vpeer)
        {
            QStringList lstCommands;
            QFile fInput(strFile);
            if (fInput.open(QFile::ReadOnly | QFile::Text))
            {
                QTextStream s(&fInput);
                while(!s.atEnd())
                {
                    QString strLine = s.readLine();
                    lstCommands.push_back(strLine);
                }
            }
            fInput.close();

            //m_layerManager.getActiveLayer()->fetchIncrementLastNodeID()

            //Execute commands
            m_dlgFieldEditor->actPopulateList(index, lstCommands);
            m_dlgFieldEditor->setModal(false);
            m_dlgFieldEditor->show();
        }
    }
    else
    {
        QMessageBox msgBox;
        msgBox.setText("Invalid file extension.");
        msgBox.exec();

        m_layerManager.removeAllLayers();
        m_layerManager.addLayer(NULL, strFilePath.ptr(), strFileTitle.ptr());
    }

    emit sig_showLayerManager(getModelLayerManager());
    updateGL();
}

void GLWidget::actFileSave()
{
    DAnsiStr strAppPath = PS::FILESTRINGUTILS::ExtractFilePath(PS::FILESTRINGUTILS::GetExePath());
    QString qstrFileName = QFileDialog::getSaveFileName(this, tr("Save Scene"), strAppPath.ptr(), tr("Blobtree Scene(*.scene)"));
    DAnsiStr strFilePath(qstrFileName.toLatin1().data());
    strFilePath = PS::FILESTRINGUTILS::ChangeFileExt(strFilePath, DAnsiStr(".scene"));
    m_layerManager.saveScript(strFilePath);
}

void GLWidget::actFileClose()
{
    resetTransformState();
    resetUndoLevels();
    //m_parsip.removeAllMPUs();
    //m_optParsip.removeAllMPUs();
    m_layerManager.selRemoveItems();
    m_layerManager.removeAllLayers();

    emit sig_showLayerManager(NULL);
    emit sig_showBlobTree(NULL);
    emit sig_showPrimitiveProperty(NULL);
    emit sig_enableHasLayer(false);
    updateGL();
}

void GLWidget::actMeshInsert()
{
    QString strFileName = QFileDialog::getOpenFileName(this, tr("Insert Mesh"),
                                                       GetExePath().ptr(),
                                                       tr("Obj Mesh(*.obj)"));

    DAnsiStr strFilePath(strFileName.toLatin1().data());
    if(PS::FILESTRINGUTILS::FileExists(strFilePath.ptr()))
    {
        DAnsiStr strFileTitle	 = PS::FILESTRINGUTILS::ExtractFileName(strFilePath);
        m_layerManager.addLayer(NULL, strFilePath.ptr(), strFileTitle.ptr());

        emit sig_showLayerManager(getModelLayerManager());
        updateGL();
    }
}

void GLWidget::actMeshSubDivide()
{
    CLayer* active = m_layerManager.getActiveLayer();
    if(active == NULL) return;
    if(!active->hasMesh()) return;

    PS::CMeshVV* aMesh = active->getMesh();

    //SubDivide
    /*
 PS::CSubDivDX11* subDiv	= new PS::CSubDivDX11(aMesh);
 subDiv->runOneShotTBB();
 subDiv->getResultAsMeshVV(aMesh);
 SAFE_DELETE(subDiv);

 emit sig_showLayerManager(getModelLayerManager());
 emit sig_addNetLog(QString("Mesh SubDivided."));
 updateGL();
        */
}

void GLWidget::actMeshPolygonize()
{
    if(m_layerManager.countLayers() == 0)
        return;

    for(size_t iLayer=0; iLayer < m_layerManager.countLayers(); iLayer++)    
        actMeshPolygonize(iLayer);

    updateGL();
}

void GLWidget::actMeshPolygonize(int idxLayer)
{
    if(!m_layerManager.isLayerIndex(idxLayer))	return;
    CLayer* aLayer = m_layerManager.getLayer(idxLayer);
    if(aLayer->getBlob() == NULL)
        return;

    //These values will be set upon each polygonization
    aLayer->setMesh();
    aLayer->setCellSize(TheAppSettings::Instance().setParsip.cellSize);
    aLayer->setAdaptiveParam(TheAppSettings::Instance().setParsip.adaptiveParam);
    aLayer->setCellShape(TheAppSettings::Instance().setParsip.cellShape);

    //Reset stats
    aLayer->getPolygonizer()->resetStats();

    if(aLayer->getTracker().isChanged())
    {
        aLayer->getTracker().reset();        
        aLayer->setOctreeFromBlobTree();
        aLayer->flattenTransformations();        
        if(aLayer->isVisible())
        {
            aLayer->setupCompactTree(aLayer->getBlob());
            aLayer->getPolygonizer()->setup(aLayer->getCompactBlobTree(),
                                            aLayer->getOctree(),
                                            idxLayer,
                                            aLayer->getCellSize());
        }

        //Run Polygonizer        
        aLayer->getPolygonizer()->run();
    }

    if(TheAppSettings::Instance().setDisplay.bShowGraph)
    {
        double total = aLayer->getPolygonizer()->statsSetupTime() +
                aLayer->getPolygonizer()->statsPolyTime();

        emit sig_setTimeFPS(total, static_cast<int>(1000.0 / total));
        emit sig_showStats(getModelStats());
        emit sig_showLayerManager(getModelLayerManager());
    }
    updateGL();
}

void GLWidget::actViewResetCamera()
{
    m_camera.goHome();
    m_globalPan.zero();
    updateGL();
}

void GLWidget::setPrimitiveColorFromColorDlg()
{
    CLayer* active = m_layerManager.getActiveLayer();
    if(active == NULL) return;
    if(active->getBlob() == NULL) return;
    if(active->selCountItems() == 0) return;
    if(active->selGetItem(0)->isOperator()) return;

    CBlobNode* sprim = active->selGetItem(0);
    if(sprim)
    {
        vec4f dif = sprim->getMaterial().diffused;
        QColor clPrev(static_cast<int>(dif.x * 255.0f),
                      static_cast<int>(dif.y * 255.0f),
                      static_cast<int>(dif.z * 255.0f),
                      static_cast<int>(dif.w * 255.0f));

        QColor clNew = QColorDialog::getColor(clPrev, this, "Select Material Color");
        if(clNew.isValid())
        {            
            float invFactor = 1.0f / 255.0f;
            vec4f diffused = vec4f(static_cast<float>(clNew.red())*invFactor,
                               static_cast<float>(clNew.green())*invFactor,
                               static_cast<float>(clNew.blue())*invFactor,
                               static_cast<float>(clNew.alpha())*invFactor);
            sprim->setMaterial(ColorToMaterial(diffused));

            active->getTracker().bump();
            actMeshPolygonize(m_layerManager.getActiveLayerIndex());

            emit sig_setPrimitiveColor(clNew);
            emit sig_showColorRibbon(getModelColorRibbon());
        }
    }
}

QStandardItemModel* GLWidget::getModelPrimitiveProperty(CBlobNode* lpNode)
{
    if(lpNode == NULL)
        return NULL;

    QList<QStandardItem*> lstRow;
    //
    m_modelBlobNodeProperty = new QStandardItemModel(this);
    m_modelBlobNodeProperty->setColumnCount(2);
    m_modelBlobNodeProperty->setHeaderData(0, Qt::Horizontal, tr("Property"));
    m_modelBlobNodeProperty->setHeaderData(1, Qt::Horizontal, tr("Value"));

    PS::PropertyList propList;
    lpNode->getProperties(propList);
    for(int i=0; i<propList.size(); i++)
    {
        lstRow.clear();
        lstRow.push_back(new QStandardItem(QString(propList[i].name.ptr())));
        lstRow.push_back(new QStandardItem(QString(propList[i].asString().ptr())));
        m_modelBlobNodeProperty->appendRow(lstRow);
    }

    if(m_bProbing)
    {
        lstRow.clear();
        lstRow.push_back(new QStandardItem(QString("Field Probe")));
        lstRow.push_back(new QStandardItem(printToQStr("%.3f", m_probeValue)));
        m_modelBlobNodeProperty->appendRow(lstRow);
    }


    /*
        case(bntOpRicciBlend):
        {
            CRicciBlend* ricci = reinterpret_cast<CRicciBlend*>(lpNode);
            lstRow.push_back(new QStandardItem(QString("Ricci Power")));
            lstRow.push_back(new QStandardItem(printToQStr("%.2f", ricci->getN())));
            m_modelBlobNodeProperty->appendRow(lstRow);
            break;
        }
        case(bntOpWarpTwist):
        {
            CWarpTwist* twist = reinterpret_cast<CWarpTwist*>(lpNode);
            lstRow.push_back(new QStandardItem(QString("Warp Factor")));
            lstRow.push_back(new QStandardItem(printToQStr("%.2f", twist->getWarpFactor())));
            m_modelBlobNodeProperty->appendRow(lstRow);
            lstRow.clear();

            lstRow.push_back(new QStandardItem(QString("Axis")));
            lstRow.push_back(new QStandardItem(QString(AxisToString(twist->getMajorAxis()).ptr())));
            m_modelBlobNodeProperty->appendRow(lstRow);
            lstRow.clear();
            break;
        }
        case(bntOpWarpTaper):
        {
            CWarpTaper* taper = reinterpret_cast<CWarpTaper*>(lpNode);
            lstRow.push_back(new QStandardItem(QString("Warp Factor")));
            lstRow.push_back(new QStandardItem(printToQStr("%.2f", taper->getWarpFactor())));
            m_modelBlobNodeProperty->appendRow(lstRow);
            lstRow.clear();

            lstRow.push_back(new QStandardItem(QString("Base Axis")));
            lstRow.push_back(new QStandardItem(QString(AxisToString(taper->getAxisAlong()).ptr())));
            m_modelBlobNodeProperty->appendRow(lstRow);
            lstRow.clear();

            lstRow.push_back(new QStandardItem(QString("Taper Axis")));
            lstRow.push_back(new QStandardItem(QString(AxisToString(taper->getAxisTaper()).ptr())));
            m_modelBlobNodeProperty->appendRow(lstRow);
            lstRow.clear();
            break;
        }
        case(bntOpWarpBend):
        {
            CWarpBend* bend = reinterpret_cast<CWarpBend*>(lpNode);
            lstRow.push_back(new QStandardItem(QString("Bend Rate")));
            lstRow.push_back(new QStandardItem(printToQStr("%.2f", bend->getBendRate())));
            m_modelBlobNodeProperty->appendRow(lstRow);
            lstRow.clear();

            lstRow.push_back(new QStandardItem(QString("Bend Center")));
            lstRow.push_back(new QStandardItem(printToQStr("%.2f", bend->getBendCenter())));
            m_modelBlobNodeProperty->appendRow(lstRow);
            lstRow.clear();

            lstRow.push_back(new QStandardItem(QString("Bend Left Bound")));
            lstRow.push_back(new QStandardItem(printToQStr("%.2f", bend->getBendRegion().left)));
            m_modelBlobNodeProperty->appendRow(lstRow);
            lstRow.clear();

            lstRow.push_back(new QStandardItem(QString("Bend Right Bound")));
            lstRow.push_back(new QStandardItem(printToQStr("%.2f", bend->getBendRegion().right)));
            m_modelBlobNodeProperty->appendRow(lstRow);
            lstRow.clear();

            break;
        }

        }

    }
    else
    {
        CSkeletonPrimitive* sprim = dynamic_cast<CSkeletonPrimitive*>(lpNode);

        vec3f s = sprim->getTransform().getScale();
        quat qq = sprim->getTransform().getRotation();
        vec3f t = sprim->getTransform().getTranslate();

        vec3f axis;
        float angle;
        qq.getAxisAngle(axis, angle);

        if(m_bProbing)
        {
            lstRow.clear();
            lstRow.push_back(new QStandardItem(QString("Field Probe")));
            lstRow.push_back(new QStandardItem(printToQStr("%.3f", m_probeValue)));
            m_modelBlobNodeProperty->appendRow(lstRow);
        }

        lstRow.clear();
        lstRow.push_back(new QStandardItem(QString("Scale")));
        lstRow.push_back(new QStandardItem(QString(Vec3ToString(s).ptr())));
        lstRow[0]->setToolTip(QString("Scale along x, y and z - Format: Scale Factor [x y z]"));
        lstRow[1]->setToolTip(QString("Scale along x, y and z - Format: Scale Factor [x y z]"));
        m_modelBlobNodeProperty->appendRow(lstRow);

        lstRow.clear();
        lstRow.push_back(new QStandardItem(QString("Rotate")));
        lstRow.push_back(new QStandardItem(QString(Vec4ToString(vec4f(angle, axis.x, axis.y, axis.z)).ptr())));
        lstRow[0]->setToolTip(QString("Rotate using angle (Degree) and axis of rotation. Format: Angle [Deg] - Axis [x y z]"));
        lstRow[1]->setToolTip(QString("Rotate using angle (Degree) and axis of rotation. Format: Angle [Deg] - Axis [x y z]"));
        m_modelBlobNodeProperty->appendRow(lstRow);

        lstRow.clear();
        lstRow.push_back(new QStandardItem(QString("Translate")));
        lstRow.push_back(new QStandardItem(QString(Vec3ToString(t).ptr())));
        lstRow[0]->setToolTip(QString("Translate along x, y and z - Format: Translate [x y z]"));
        lstRow[1]->setToolTip(QString("Translate along x, y and z - Format: Translate [x y z]"));
        m_modelBlobNodeProperty->appendRow(lstRow);

        //
        lstRow.clear();
        SkeletonType sktType = sprim->getSkeleton()->getType();
        switch(sktType)
        {
        case(sktRing):
        {
            CSkeletonRing* ring = dynamic_cast<CSkeletonRing*>(sprim->getSkeleton());
            lstRow.push_back(new QStandardItem(QString("Radius")));
            lstRow.push_back(new QStandardItem(printToQStr("%.2f", ring->getRadius())));
            m_modelBlobNodeProperty->appendRow(lstRow);
            break;
        }
        case(sktDisc):
        {
            CSkeletonDisc* disc = dynamic_cast<CSkeletonDisc*>(sprim->getSkeleton());
            lstRow.push_back(new QStandardItem(QString("Radius")));
            lstRow.push_back(new QStandardItem(printToQStr("%.2f", disc->getRadius())));
            m_modelBlobNodeProperty->appendRow(lstRow);
            break;
        }
        case(sktCylinder):
        {
            CSkeletonCylinder* cyl = dynamic_cast<CSkeletonCylinder*>(sprim->getSkeleton());
            lstRow.push_back(new QStandardItem(QString("Radius")));
            lstRow.push_back(new QStandardItem(printToQStr("%.2f", cyl->getRadius())));
            m_modelBlobNodeProperty->appendRow(lstRow);

            lstRow.clear();
            lstRow.push_back(new QStandardItem(QString("Height")));
            lstRow.push_back(new QStandardItem(printToQStr("%.2f", cyl->getHeight())));
            m_modelBlobNodeProperty->appendRow(lstRow);
            break;
        }
        case(sktCube):
        {
            CSkeletonCube* cube = dynamic_cast<CSkeletonCube*>(sprim->getSkeleton());
            lstRow.push_back(new QStandardItem(QString("Side")));
            lstRow.push_back(new QStandardItem(printToQStr("%.2f", cube->getSide())));
            m_modelBlobNodeProperty->appendRow(lstRow);
            break;
        }
        }

        //Show current color
        vec4f dif = sprim->getMaterial().diffused;
        QColor cl(static_cast<int>(dif.x * 255.0f),
                  static_cast<int>(dif.y * 255.0f),
                  static_cast<int>(dif.z * 255.0f),
                  static_cast<int>(dif.w * 255.0f));
        emit sig_setPrimitiveColor(cl);
    }
*/    
    connect(m_modelBlobNodeProperty, SIGNAL(dataChanged(QModelIndex, QModelIndex)), this, SLOT(dataChanged_tblBlobProperty(QModelIndex, QModelIndex)));
    return m_modelBlobNodeProperty;
}

void GLWidget::dataChanged_tblBlobProperty(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    //Check active layer
    if(!m_layerManager.hasActiveLayer()) return;

    //Check which column sent edit signal
    int row = topLeft.row();
    int col = topLeft.column();
    if(col != 1) return;

    PropertyList lstProps;
    //Val, Name
    lstProps.add(DAnsiStr(m_modelBlobNodeProperty->item(row, 1)->text().toLatin1()),
                 m_modelBlobNodeProperty->item(row, 0)->text().toLatin1());
    if(m_layerManager.getActiveLayer()->selCountItems() > 0)
    {
        m_layerManager.getActiveLayer()->selGetItem(0)->setProperties(lstProps);
    }

    //Set Affine things
    /*
    QString strValue = m_modelBlobNodeProperty->itemFromIndex(topLeft)->text();
    if(m_lpSelectedBlobNode->isOperator())
    {
        switch(m_lpSelectedBlobNode->getNodeType())
        {
        case(bntOpPCM):
        {
            CPcm* lpPCM = reinterpret_cast<CPcm*>(m_lpSelectedBlobNode);
            if(row == 0)
                lpPCM->setPropagateLeft(atof(strValue.toLatin1()));
            else if(row == 1)
                lpPCM->setPropagateRight(atof(strValue.toLatin1()));
            else if(row == 2)
                lpPCM->setAlphaLeft(atof(strValue.toLatin1()));
            else if(row == 3)
                lpPCM->setAlphaRight(atof(strValue.toLatin1()));


            break;
        }
        case(bntOpRicciBlend):
        {
            CRicciBlend* ricci = reinterpret_cast<CRicciBlend*>(m_lpSelectedBlobNode);
            float n = atof(strValue.toLatin1());
            ricci->setN(n);
            break;
        }
        case(bntOpWarpTwist):
        {
            CWarpTwist* twist = reinterpret_cast<CWarpTwist*>(m_lpSelectedBlobNode);
            if(row == 0)
            {
                float n = atof(strValue.toLatin1());
                twist->setWarpFactor(n);
            }
            else
            {
                MajorAxices axis = StringToAxis(DAnsiStr(strValue.toLatin1()));
                twist->setMajorAxis(axis);
            }
            break;
        }
        case(bntOpWarpTaper):
        {
            CWarpTaper* taper = reinterpret_cast<CWarpTaper*>(m_lpSelectedBlobNode);
            if(row == 0)
            {
                float n = atof(strValue.toLatin1());
                taper->setWarpFactor(n);
            }
            else if(row == 1)
            {
                MajorAxices axisAlong = StringToAxis(DAnsiStr(strValue.toLatin1()));
                taper->setAxisAlong(axisAlong);
            }
            else if(row == 2)
            {
                MajorAxices axisTaper = StringToAxis(DAnsiStr(strValue.toLatin1()));
                taper->setAxisTaper(axisTaper);
            }
            break;
        }
        case(bntOpWarpBend):
        {
            //Rate, Center, Region
            CWarpBend* bend = reinterpret_cast<CWarpBend*>(m_lpSelectedBlobNode);
            if(row == 0)
            {
                float r = atof(strValue.toLatin1());
                bend->setBendRate(r);
            }
            else if(row == 1)
            {
                float c = atof(strValue.toLatin1());
                bend->setBendCenter(c);
            }
            else if(row == 2)
            {
                //Bend Left
                float left = atof(strValue.toLatin1());
                bend->setBendRegionLeft(left);
            }
            else if(row == 3)
            {
                //Bend Left
                float right = atof(strValue.toLatin1());
                bend->setBendRegionRight(right);
            }
            break;
        }

        }
    }
    else
    {
        //1.Set Affine related changes
        CSkeletonPrimitive* sprim = reinterpret_cast<CSkeletonPrimitive*>(m_lpSelectedBlobNode);
        if(row < 3)
        {
            //Scale
            if(row == 0)
            {
                vec3f scale = StringToVec3(DAnsiStr(strValue.toLatin1()));
                sprim->getTransform().setScale(scale);
            }
            else if(row == 1)
            {
                vec4f rot = StringToVec4(DAnsiStr(strValue.toLatin1()));
                quat q;
                q.fromAngleAxis(DEGTORAD(rot.x), vec3f(rot.y, rot.z, rot.w));
                sprim->getTransform().setRotation(q);

            }
            else if(row == 2)
            {
                vec3f translate = StringToVec3(DAnsiStr(strValue.toLatin1()));
                sprim->getTransform().setTranslate(translate);
            }
        }
        //2.Set Skeleton related changes
        else if(row >= 3)
        {
            switch(sprim->getSkeleton()->getType())
            {
            case(sktRing):
            {
                CSkeletonRing* ring = reinterpret_cast<CSkeletonRing*>(sprim->getSkeleton());
                float r = atof(strValue.toLatin1());
                ring->setRadius(r);
                break;
            }
            case(sktDisc):
            {
                CSkeletonDisc* disc = reinterpret_cast<CSkeletonDisc*>(sprim->getSkeleton());
                float r = atof(strValue.toLatin1());
                disc->setRadius(r);
                break;
            }
            case(sktCylinder):
            {
                CSkeletonCylinder* cyl = reinterpret_cast<CSkeletonCylinder*>(sprim->getSkeleton());
                if(row == 3)
                {
                    //Radius
                    float r = atof(strValue.toLatin1());
                    cyl->setRadius(r);
                }
                else if(row == 4)
                {
                    //Height
                    float h = atof(strValue.toLatin1());
                    cyl->setHeight(h);
                }
                break;
            }
            case(sktCube):
            {
                CSkeletonCube* cube = reinterpret_cast<CSkeletonCube*>(sprim->getSkeleton());
                float r = atof(strValue.toLatin1());
                cube->setSide(r);
                break;
            }
            }
        }
    }
    */

    //Bump Version
    m_layerManager.getActiveLayer()->getTracker().bump();

    //Repolygonize the model since some changes occured in properties
    actMeshPolygonize(m_layerManager.getActiveLayerIndex());
}


QStandardItemModel * GLWidget::getModelBlobTree(int iLayer)
{
    if(!m_layerManager.isLayerIndex(iLayer))
        return NULL;
    //Create an instance of treeModel
    SAFE_DELETE(m_modelBlobTree);
    QString strTitle = QString("BlobTree Layer#%1").arg(m_layerManager.getLayer(iLayer)->getGroupName().ptr());
    m_modelBlobTree = new QStandardItemModel(this);
    m_modelBlobTree->setColumnCount(1);
    m_modelBlobTree->setHeaderData(0, Qt::Horizontal, strTitle);


    std::list< std::pair <CBlobNode*, QStandardItem *> > stkProcessing;
    QStandardItem * parentItem = m_modelBlobTree->invisibleRootItem();

    CBlobNode * blobParent  = NULL;
    CBlobNode * blobChild  = NULL;


    DAnsiStr strLayerName;
    char chrBlobNodeName[MAX_NAME_LEN];
    std::pair <CBlobNode*, QStandardItem*> mypair;
    QString strTag;

    //m_layerManager[iLayer]->getSelPrimitive()
    //First Node
    blobParent = m_layerManager[iLayer]->getBlob();
    if(blobParent != NULL)
    {
        strLayerName = m_layerManager[iLayer]->getGroupName();
        strTag = QString("Layer: %1 RootNode: %2:%3").arg(strLayerName.ptr()).arg(blobParent->getID()).arg(blobParent->getName().c_str());

        QStandardItem * childItem = new QStandardItem(strTag);
        childItem->setEditable(true);
        childItem->setData(reinterpret_cast<U64>(blobParent));
        parentItem->appendRow(childItem);

        //Stack to Process items without using recursion
        mypair.first = blobParent;
        mypair.second = childItem;

        stkProcessing.push_front(mypair);
    }

    while(!stkProcessing.empty())
    {
        blobParent = static_cast<CBlobNode*>(stkProcessing.front().first);
        parentItem = static_cast<QStandardItem*>(stkProcessing.front().second);
        stkProcessing.pop_front();

        //Push this node children to stack
        for(size_t i=0; i <	blobParent->countChildren(); i++)
        {
            blobChild = blobParent->getChild(i);

            //Set this node
            QStandardItem *item = new QStandardItem(QString("%1:%2").arg(blobChild->getID()).arg(blobChild->getName().c_str()));
            item->setEditable(true);
            item->setData(reinterpret_cast<U64>(blobChild));
            parentItem->appendRow(item);

            mypair.first = blobChild;
            mypair.second = item;

            stkProcessing.push_front(mypair);
        }
    }

    connect(m_modelBlobTree, SIGNAL(dataChanged(QModelIndex, QModelIndex)), this, SLOT(dataChanged_treeBlob(QModelIndex, QModelIndex)));
    return m_modelBlobTree;
}


void GLWidget::dataChanged_treeBlob(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    QString str = m_modelBlobTree->itemFromIndex(topLeft)->text();
}

QStandardItemModel* GLWidget::getModelLayerManager()
{
    if(m_layerManager.countLayers() == 0)
        return NULL;

    SAFE_DELETE(m_modelLayerManager);
    m_modelLayerManager = new QStandardItemModel(this);
    m_modelLayerManager->setColumnCount(3);
    m_modelLayerManager->setHeaderData(0, Qt::Horizontal, tr("Layer Name"));
    m_modelLayerManager->setHeaderData(1, Qt::Horizontal, tr("Mesh"));
    m_modelLayerManager->setHeaderData(2, Qt::Horizontal, tr("Visible"));

    //QStandardItem* parentItem = m_modelLayerManager->invisibleRootItem();

    size_t ctLayers = m_layerManager.countLayers();
    QList<QStandardItem*> lstRow;
    QStandardItem* item = NULL;
    DAnsiStr strMeshInfo;
    size_t ctVertices, ctFaces;
    CLayer* aLayer = NULL;
    for(size_t i=0; i<ctLayers; i++)
    {
        aLayer = m_layerManager[i];
        //m_parsip.getMeshStats(i, ctVertices, ctFaces);
        if(aLayer->hasMesh())
            aLayer->getMeshInfo(ctVertices, ctFaces);
        else
            aLayer->getPolygonizer()->statsMeshInfo(ctVertices, ctFaces);

        lstRow.clear();
        strMeshInfo = PS::printToAStr("F#%i, V#%i", ctFaces, ctVertices);
        lstRow.push_back(new QStandardItem(QString(aLayer->getGroupName().ptr())));
        lstRow.push_back(new QStandardItem(QString(strMeshInfo.ptr())));

        //
        item = new QStandardItem(QString(aLayer->isVisible()));
        item->setCheckable(true);
        item->setCheckState(aLayer->isVisible()?Qt::Checked:Qt::Unchecked);
        lstRow.push_back(item);

        m_modelLayerManager->appendRow(lstRow);
    }

    connect(m_modelLayerManager, SIGNAL(dataChanged(QModelIndex, QModelIndex)), this, SLOT(dataChanged_tblLayers(QModelIndex, QModelIndex)));
    return m_modelLayerManager;
}

//Handle Changes to Model
void GLWidget::dataChanged_tblLayers( const QModelIndex& topLeft, const QModelIndex& bottomRight )
{
    int row = topLeft.row();
    if(!m_layerManager.isLayerIndex(row)) return;

    int col = topLeft.column();
    if(col == 0)
    {
        QString str = m_modelLayerManager->itemFromIndex(topLeft)->text();
        m_layerManager[row]->setGroupName(DAnsiStr(str.toLatin1()));
    }
    else if(col == 2)
    {
        bool bVis = (m_modelLayerManager->itemFromIndex(topLeft)->checkState() == Qt::Checked);
        m_layerManager[row]->setVisible(bVis);
        m_layerManager[row]->getTracker().bump();
        updateGL();
    }
}


QStandardItemModel* GLWidget::getModelStats()
{
    if(m_layerManager.countLayers() == 0)
        return NULL;
    SAFE_DELETE(m_modelStats);

    CParsipOptimized* parsip = m_layerManager[0]->getPolygonizer();
    m_modelStats = new QStandardItemModel(this);
    m_modelStats->setColumnCount(2);
    //m_modelStats->setHeaderData(0, Qt::Horizontal, tr("#"));
    m_modelStats->setHeaderData(0, Qt::Horizontal, tr("item"));
    m_modelStats->setHeaderData(1, Qt::Horizontal, tr("value"));

    QList<QStandardItem*> lstRow;
    lstRow.push_back(new QStandardItem(QString("MPUs Intersected/Total")));
    lstRow.push_back(new QStandardItem(QString("%1/%2").arg(parsip->statsIntersectedMPUs()).arg(parsip->countMPUs())));
    m_modelStats->appendRow(lstRow);
    lstRow.clear();

    lstRow.push_back(new QStandardItem(QString("Cells Intersected/Total")));
    lstRow.push_back(new QStandardItem(QString("%1/%2").arg(parsip->statsIntersectedCellsCount()).arg(parsip->statsTotalCellsInIntersectedMPUs())));
    m_modelStats->appendRow(lstRow);
    lstRow.clear();

    size_t ctVertices, ctFaces;
    //m_parsip.getMeshStats(ctVertices, ctFaces);
    parsip->statsMeshInfo(ctVertices, ctFaces);
    lstRow.push_back(new QStandardItem(QString("Mesh")));
    lstRow.push_back(new QStandardItem(QString("T=%1, V=%2").arg(ctFaces).arg(ctVertices)));
    m_modelStats->appendRow(lstRow);
    lstRow.clear();



    //int ctIntersectedMPUs = m_parsip.getIntersectedMPUs();
    int ctIntersectedMPUs = parsip->statsIntersectedMPUs();
    //int ctSurfaceTrackedMPUs = m_parsip.getSurfaceTrackedMPUs();
    int ctSurfaceTrackedMPUs = 0;
    lstRow.push_back(new QStandardItem(QString("Tracking and MC")));
    lstRow.push_back(new QStandardItem(QString("ST=%1; MC=%2").arg(ctSurfaceTrackedMPUs).arg(ctIntersectedMPUs - ctSurfaceTrackedMPUs)));
    m_modelStats->appendRow(lstRow);
    lstRow.clear();


    float percentST = (float)ctSurfaceTrackedMPUs / (float)ctIntersectedMPUs;
    float percentMC = 1.0f -  percentST;
    lstRow.push_back(new QStandardItem(QString("Tracking and MC Percentage")));
    lstRow.push_back(new QStandardItem(QString("ST=%1%; MC=%2%").arg(floorf(percentST * 100.0f)).arg(floorf(percentMC * 100.0f))));
    m_modelStats->appendRow(lstRow);
    lstRow.clear();

    lstRow.push_back(new QStandardItem(QString("Total Field Eval#")));
    //lstRow.push_back(new QStandardItem(QString("%1").arg(m_parsip.getFieldEvalStats())));
    lstRow.push_back(new QStandardItem(QString("%1").arg(parsip->statsTotalFieldEvals())));
    m_modelStats->appendRow(lstRow);
    lstRow.clear();



    lstRow.push_back(new QStandardItem(QString("FieldEval/Triangle")));
    //lstRow.push_back(new QStandardItem(QString("%1").arg((int)(ctFaces > 0?(m_parsip.getFieldEvalStats() / ctFaces):0))));
    lstRow.push_back(new QStandardItem(QString("%1").arg((int)(ctFaces > 0?(parsip->statsTotalFieldEvals() / ctFaces):0))));
    m_modelStats->appendRow(lstRow);
    lstRow.clear();


    //CMpu* latestMPU = m_parsip.getLastestMPU();
    CSIMDMPU* latestMPU = parsip->statsLatestMPU();

    if(latestMPU)
    {
        DAnsiStr strLatest = printToAStr("%.2f", latestMPU->statsProcessTime());
        lstRow.push_back(new QStandardItem(QString("Latest MPU Time")));
        lstRow.push_back(new QStandardItem(QString(strLatest.ptr())));
        m_modelStats->appendRow(lstRow);
        lstRow.clear();

        strLatest = printToAStr("C#%d, FE=%d", latestMPU->statsIntersectedCells(), latestMPU->statsFieldEvals());
        lstRow.push_back(new QStandardItem(QString("Latest MPU Cells/FieldEval")));
        lstRow.push_back(new QStandardItem(QString(strLatest.ptr())));
        m_modelStats->appendRow(lstRow);
        lstRow.clear();

    }

    //Find number of cells for complete model
    if(m_layerManager.countLayers() > 0)
    {
        float cellsize = m_layerManager[0]->getCellSize();
        COctree oct = m_layerManager[0]->getOctree();
        for(size_t i=1; i<m_layerManager.countLayers(); i++)
            oct.csgUnion(m_layerManager[i]->getOctree());
        vec3f sides = oct.getSidesSize();
        vec3i res = vec3i(static_cast<int>(ceil(sides.x / cellsize)),
                          static_cast<int>(ceil(sides.y / cellsize)),
                          static_cast<int>(ceil(sides.z / cellsize)));

        lstRow.push_back(new QStandardItem(QString("CellsResolution")));
        lstRow.push_back(new QStandardItem(QString("[%1, %2, %3]").arg(res.x).arg(res.y).arg(res.z)));
        m_modelStats->appendRow(lstRow);
        lstRow.clear();
    }

    //double u[8];
    //m_parsip.getCoreUtilizations(u, 8);

    return m_modelStats;
}

QStandardItemModel* GLWidget::getModelColorRibbon()
{
    SAFE_DELETE(m_modelColorRibbon);

    m_modelColorRibbon = new QStandardItemModel(this);
    m_modelColorRibbon->setColumnCount(8);
    QList<QStandardItem*> lstRow;

    QStandardItem* item = NULL;
    for(int i=0; i<8; i++)
    {
        m_modelColorRibbon->setHeaderData(i, Qt::Horizontal, QString("Color %1").arg(i+1));

        item = new QStandardItem(QString("Color %1").arg(i+1));
        vec4f bgColor = m_materials[i].diffused * (255.0f, 255.0f, 255.0f, 255.0f);
        vec4f fgColor = TextColor(m_materials[i].diffused) * (255.0f, 255.0f, 255.0f, 255.0f);

        item->setBackground(QBrush(QColor((int)bgColor.x, (int)bgColor.y, (int)bgColor.z, (int)bgColor.w)));
        item->setForeground(QBrush(QColor((int)fgColor.x, (int)fgColor.y, (int)fgColor.z, (int)fgColor.w)));
        lstRow.push_back(item);
    }
    m_modelColorRibbon->appendRow(lstRow);

    connect(m_modelColorRibbon, SIGNAL(dataChanged(QModelIndex, QModelIndex)), this, SLOT(dataChanged_tblColorRibbon(QModelIndex, QModelIndex)));

    return m_modelColorRibbon;
}

void GLWidget::edit_tblColorRibbon()
{
    vec4f dif = m_materials[m_idxRibbonSelection].diffused;
    QColor clPrev(static_cast<int>(dif.x * 255.0f),
                  static_cast<int>(dif.y * 255.0f),
                  static_cast<int>(dif.z * 255.0f),
                  static_cast<int>(dif.w * 255.0f));

    QColor clNew = QColorDialog::getColor(clPrev, this, "Select Material Color");
    CMaterial m;
    float invFactor = 1.0f / 255.0f;
    m.diffused = vec4f(static_cast<float>(clNew.red())*invFactor,
                       static_cast<float>(clNew.green())*invFactor,
                       static_cast<float>(clNew.blue())*invFactor,
                       static_cast<float>(clNew.alpha())*invFactor);
    m.ambient = m.diffused * 0.5f;
    m.specular = vec4f(0.8f, 0.8f, 0.8f, 0.8f) + m.diffused * 0.2f;
    m.shininess = 32.0f;

    m_materials[m_idxRibbonSelection]  = m;

    emit sig_showColorRibbon(getModelColorRibbon());
}

void GLWidget::select_tblColorRibbon(const QModelIndex& topLeft)
{
    CLayer* active = m_layerManager.getActiveLayer();
    if(active == NULL) return;
    if(active->selCountItems() == 0) return;
    if(active->selGetItem(0)->getNodeType() != bntPrimSkeleton) return;

    CBlobNode* sprim = active->selGetItem(0);
    if(sprim)
    {
        m_idxRibbonSelection = topLeft.column();
        vec4f dif = m_materials[m_idxRibbonSelection].diffused * (255.0f, 255.0f, 255.0f, 255.0f);
        sprim->setMaterial(m_materials[m_idxRibbonSelection]);

        active->getTracker().bump();
        emit sig_setPrimitiveColor(QColor((int)dif.x, (int)dif.y, (int)dif.z, (int)dif.w));
        emit sig_showColorRibbon(getModelColorRibbon());
        actMeshPolygonize(m_layerManager.getActiveLayerIndex());
    }
}

void GLWidget::selectBlobNode(int iLayer, CBlobNode* aNode, bool bMultiSelect)
{
    if((aNode == NULL)||(!m_layerManager.isLayerIndex(iLayer))) return;

    //if MultiSelection is disabled then clear all selection lists
    if(!bMultiSelect)
        m_layerManager.selRemoveItems();
    m_layerManager[iLayer]->selAddItem(aNode);

    //Goto Translate mode
    actEditTranslate();

    //Show Properties
    emit sig_showPrimitiveProperty(getModelPrimitiveProperty(aNode));
    updateGL();
}

void GLWidget::selectBlobNode(QModelIndex idx)
{
    if(m_modelBlobTree == NULL) return;

    QStandardItem* item = m_modelBlobTree->itemFromIndex(idx);
    if(item == NULL) return;

    bool bMultiSelect = false;
    if(QApplication::keyboardModifiers() == Qt::ControlModifier)
        bMultiSelect = true;


    //Fetch node pointer from its QStandardItem    
    CBlobNode* lpNode = (CBlobNode*)(item->data().toInt());
    if(lpNode != NULL)
        selectBlobNode(m_layerManager.getActiveLayerIndex(), lpNode, bMultiSelect);

    //Goto transform mode
    actEditTranslate();
}


void GLWidget::selectLayer(int iLayer)
{
    m_layerManager.setActiveLayer(iLayer);
    emit sig_enableHasLayer(true);
    emit sig_showBlobTree(getModelBlobTree(iLayer));
    updateGL();
}

void GLWidget::selectLayer( QModelIndex idx )
{
    selectLayer(idx.row());
}


void GLWidget::actLayerAdd()
{
    DAnsiStr strLayerName = PS::printToAStr("Layer %d", m_layerManager.countLayers());
    m_layerManager.addLayer(NULL, strLayerName.ptr());
    emit sig_showLayerManager(getModelLayerManager());
}

void GLWidget::actLayerDelete()
{
    if(m_layerManager.getActiveLayer())
    {
        //m_parsip.removeLayerMPUs(m_layerManager.getActiveLayerIndex());
        m_layerManager.removeLayer(m_layerManager.getActiveLayerIndex());
        emit sig_showLayerManager(getModelLayerManager());
        emit sig_showBlobTree(NULL);
        emit sig_enableHasLayer(m_layerManager.countLayers() > 0);
        updateGL();
    }
}

void GLWidget::actLayerDuplicate()
{
    CLayer* aLayer = m_layerManager.getActiveLayer();
    if(aLayer != NULL)
    {
        //Create a Memory stream for Blob Content
        CSketchConfig* lpSketchConfig = new CSketchConfig();
        aLayer->getBlob()->saveScript(lpSketchConfig);

        //Create the new layer
        DAnsiStr strLayerName = PS::printToAStr("Layer %d", m_layerManager.countLayers());
        m_layerManager.addLayer(NULL, strLayerName.ptr());
        aLayer = m_layerManager.getLast();
        aLayer->recursive_ReadBlobNode(lpSketchConfig, NULL, 0);
        aLayer->getTracker().bump();

        //Delete memory stream
        SAFE_DELETE(lpSketchConfig);

        emit sig_showLayerManager(getModelLayerManager());
        emit sig_enableHasLayer(m_layerManager.countLayers() > 0);

        actMeshPolygonize();
    }
}

void GLWidget::actLayerSelectAll()
{
    CLayer* aLayer = m_layerManager.getActiveLayer();
    if(aLayer != NULL)
    {

    }
}


void GLWidget::actAddSphere()
{
    m_sketchType = bntPrimPoint;
    m_uiMode = uimSketch;
}

void GLWidget::actAddCylinder()
{
    m_sketchType = bntPrimCylinder;
    m_uiMode = uimSketch;
}

void GLWidget::actAddRing()
{
    m_sketchType = bntPrimRing;
    m_uiMode = uimSketch;
}

void GLWidget::actAddDisc()
{
    m_sketchType = bntPrimDisc;
    m_uiMode = uimSketch;
}

void GLWidget::actAddCube()
{
    m_sketchType = bntPrimCube;
    m_uiMode = uimSketch;
}

void GLWidget::actAddTriangle()
{
    m_sketchType = bntPrimTriangle;
    m_uiMode = uimSketch;
}

void GLWidget::actAddInstance()
{
    m_sketchType = bntPrimInstance;
    m_uiMode = uimSketch;
}


void GLWidget::actAddPolygonPlane()
{
    m_lstSketchControlPoints.resize(0);
    m_uiMode = uimSketch;
    m_sketchType = bntPrimPolygon;
}

void GLWidget::actAddWarpTwist()
{
    addBlobOperator(bntOpWarpTwist);
}

void GLWidget::actAddWarpTaper()
{
    addBlobOperator(bntOpWarpTaper);
}

void GLWidget::actAddWarpBend()
{
    addBlobOperator(bntOpWarpBend);
}

void GLWidget::actAddUnion()
{
    addBlobOperator(bntOpUnion);
}

void GLWidget::actAddPCM()
{
    addBlobOperator(bntOpPCM);
}

void GLWidget::actAddGradientBlend()
{
    addBlobOperator(bntOpGradientBlend);
}

void GLWidget::actAddIntersection()
{
    addBlobOperator(bntOpIntersect);
}

void GLWidget::actAddDifference()
{
    addBlobOperator(bntOpDif);
}

void GLWidget::actAddSmoothDif()
{
    addBlobOperator(bntOpSmoothDif);
}

void GLWidget::actAddBlend()
{
    addBlobOperator(bntOpBlend);
}

void GLWidget::actAddRicciBlend()
{
    addBlobOperator(bntOpRicciBlend);
}

void GLWidget::actAddCacheNode()
{
    addBlobOperator(bntOpCache);
}

void GLWidget::actViewCamLeftKey(bool bEnable)
{
    m_bEnableCamLeftKey = bEnable;
}

void GLWidget::actViewZoomIn()
{
    m_camera.setZoom(m_camera.getCurrentZoom() + 1);
    emit sig_viewZoomChanged(m_camera.getCurrentZoom());
    updateGL();
}

void GLWidget::actViewZoomOut()
{
    m_camera.setZoom(m_camera.getCurrentZoom() - 1);
    emit sig_viewZoomChanged(m_camera.getCurrentZoom());
    updateGL();
}

void GLWidget::actViewSetZoom(int value)
{
    m_camera.setZoom((float)value);
    updateGL();
}

void GLWidget::actEditSelect()
{
    m_uiMode = uimSelect;
}

void GLWidget::actEditTranslate()
{
    m_uiMode = uimTransform;
    TheUITransform::Instance().type = uitTranslate;
    TheUITransform::Instance().axis = uiaFree;
    TheUITransform::Instance().translate.zero();

    SAFE_DELETE(m_lpUIWidget);
    m_lpUIWidget = new CMapTranslate();
    updateGL();
}

void GLWidget::actEditRotate()
{
    m_uiMode = uimTransform;
    TheUITransform::Instance().type = uitRotate;
    TheUITransform::Instance().axis = uiaFree;

    SAFE_DELETE(m_lpUIWidget);
    m_lpUIWidget = new CMapRotate();
    updateGL();
}

void GLWidget::actEditScale()
{
    m_uiMode = uimTransform;
    TheUITransform::Instance().type = uitScale;
    TheUITransform::Instance().axis = uiaFree;

    SAFE_DELETE(m_lpUIWidget);
    m_lpUIWidget = new CMapScale();
    updateGL();
}

void GLWidget::actEditAxisX()
{
    TheUITransform::Instance().axis = uiaX;
    if(m_lpUIWidget)
        m_lpUIWidget->createWidget();
    updateGL();
}

void GLWidget::actEditAxisY()
{
    TheUITransform::Instance().axis = uiaY;
    if(m_lpUIWidget)
        m_lpUIWidget->createWidget();
    updateGL();
}

void GLWidget::actEditAxisZ()
{
    TheUITransform::Instance().axis = uiaZ;
    if(m_lpUIWidget)
        m_lpUIWidget->createWidget();
    updateGL();
}

void GLWidget::actEditAxisFree()
{
    TheUITransform::Instance().axis = uiaFree;
    if(m_lpUIWidget)
        m_lpUIWidget->createWidget();
    updateGL();
}

void GLWidget::actEditDelete()
{
    CLayer* active = m_layerManager.getActiveLayer();
    if(active == NULL) return;

    CBlobNode* node = active->selGetItem(0);
    if(node == NULL) return;

    active->selRemoveItem(0);
    if(node == active->getBlob())
    {
        active->cleanup();
        //m_optParsip.removeAllMPUs();
    }
    else
    {
        if(active->recursive_ExecuteCmdBlobtreeNode(active->getBlob(), node, cbtDelete))
        {
            active->getTracker().bump();
            actMeshPolygonize(m_layerManager.getActiveLayerIndex());
        }
    }
    emit sig_showPrimitiveProperty(NULL);
    emit sig_showBlobTree(NULL);
}

//Add a new skeletal primitive to active layer's BlobTree
//Removes previous blobtree if it was a primitive node
//Adds itself as a child of root operator
CBlobNode* GLWidget::addBlobPrimitive(BlobNodeType primitiveType, const vec3f& pos, int preferredID, bool bSendToNet)
{
    CLayer* aLayer = m_layerManager.getActiveLayer();
    if(aLayer == NULL) return NULL;
    if(primitiveType == bntPrimInstance)
    {
        if(aLayer->getBlob() == NULL)
        {
            ReportError("Instanced Primitive cannot be added as a first primitive.");
            FlushAllErrors();
            return NULL;
        }
        else if(aLayer->selCountItems() == 0)
        {
            ReportError("Instanced Primitive require a selected sub-tree. [Select a Node]");
            FlushAllErrors();
            return NULL;
        }
    }


    //Capture Undo Level
    addUndoLevel();

    CBlobNode* primitive = NULL;
    try{
        primitive = TheBlobNodeFactoryIndex::Instance().CreateObject(primitiveType);
        if(aLayer->getBlob() == NULL)
        {
            //Create a global union node first
            CBlobNode* globalUnion = TheBlobNodeFactoryIndex::Instance().CreateObject(bntOpUnion);
            globalUnion->setID(aLayer->getIDDispenser().bump());
            aLayer->setBlob(globalUnion);            
        }
    }
    catch(exception e)
    {
        ReportError(e.what());
    }

    //Create SkeletonPrimitive
    //Set original node for instanced primitive
    if(primitiveType == bntPrimInstance)
    {
        reinterpret_cast<CInstance*>(primitive)->setOriginalNode(aLayer->selGetItem(0));
        primitive->getTransform().addTranslate(pos - aLayer->selGetItem(0)->getOctree().center());
    }
    else
        primitive->getTransform().addTranslate(pos);


    primitive->setMaterial(m_materials[m_idxRibbonSelection]);
    if(preferredID >= 0)
        primitive->setID(preferredID);
    else
        primitive->setID(aLayer->getIDDispenser().bump());
    aLayer->getBlob()->addChild(primitive);

    if(bSendToNet)
    {
        actNetSendCommand(cmdAdd, primitive, primitive->getMaterial().diffused);
        actNetSendCommand(cmdMove, primitive, vec4f(pos.x, pos.y, pos.z));
    }

    //Set revision
    aLayer->getTracker().bump();

    int idxLayer = m_layerManager.getActiveLayerIndex();
    actMeshPolygonize(idxLayer);
    emit sig_showBlobTree(getModelBlobTree(idxLayer));

    return primitive;
}

CBlobNode* GLWidget::addBlobOperator(BlobNodeType operatorType, int preferredID, bool bSendToNet)
{
    CLayer* aLayer = m_layerManager.getActiveLayer();
    if(aLayer == NULL) return NULL;
    if(aLayer->getBlob() == NULL) return NULL;
    if(aLayer->selCountItems() == 0) return NULL;

    //Create operator
    CBlobNode* op = TheBlobNodeFactoryIndex::Instance().CreateObject(operatorType);
    bool bIsUnary = op->isUnary();

    //If not a unary operator then we need two selected nodes
    if((!bIsUnary) && (aLayer->selCountItems() < 2))
    {
        ReportError("Select two blobby nodes and then perform this operation.");
        FlushAllErrors();
        return NULL;
    }

    //Assign and Increment ID
    if(preferredID >= 0)
        op->setID(preferredID);
    else
        op->setID(aLayer->getIDDispenser().bump());

    //Find parent and update relationships
    CBlobNode* dstParent = NULL;
    for(size_t i=0; i<aLayer->selCountItems(); i++)
    {
        CBlobNode* lpOutParent = NULL;
        if(aLayer->actBlobFindParent(aLayer->selGetItem(i), lpOutParent))
        {
            lpOutParent->detachChild(aLayer->selGetItem(i));
            if(i == 0)
                dstParent = lpOutParent;
        }
        else
        {
            ReportError("Unable to find the parent of selected node(s) to perform this operation.");
            FlushAllErrors();
            SAFE_DELETE(op);
            return NULL;
        }
    }

    //Capture Undo Level
    addUndoLevel();


    //Perform Transaction
    dstParent->addChild(op);

    if(bIsUnary)
        op->addChild(aLayer->selGetItem(0));
    else
    {
        for(size_t i=0; i<aLayer->selCountItems(); i++)
        {
            op->addChild(aLayer->selGetItem(i));
        }
    }


    //Send to members in DesignNET
    if(bSendToNet &&(CDesignNet::GetDesignNet()->countMembers() > 0))
        actNetSendCommand(cmdOperator, op, op->getMaterial().diffused);

    aLayer->getTracker().bump();

    int idxLayer = m_layerManager.getActiveLayerIndex();

    actMeshPolygonize(idxLayer);
    emit sig_showBlobTree(getModelBlobTree(idxLayer));

    return op;
}

void GLWidget::drawPolygon( const vector<vec3f>& lstPoints )
{
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    CMeshVV::setOglMaterial(CMaterial::mtrlBlack());
    glPointSize(5.0f);
    glBegin(GL_POINTS);
    for(size_t i=0; i<lstPoints.size(); i++)
        glVertex3fv(lstPoints[i].ptr());
    glEnd();

    glLineWidth(2.0f);
    //glLineStipple(
    glBegin(GL_LINE_STRIP);
    for(size_t i=0; i<lstPoints.size(); i++)
        glVertex3fv(lstPoints[i].ptr());
    glEnd();
    glPopAttrib();
}

bool GLWidget::windowToObject( vec3f window, vec3f& object )
{
    GLdouble ox, oy, oz;
    GLdouble mv[16];
    GLdouble pr[16];
    GLint vp[4];

    glGetDoublev(GL_MODELVIEW_MATRIX, mv);
    glGetDoublev(GL_PROJECTION_MATRIX, pr);
    glGetIntegerv(GL_VIEWPORT, vp);
    if(gluUnProject(window.x, window.y, window.z, mv, pr, vp, &ox, &oy, &oz) == GL_TRUE)
    {
        object = vec3f(ox, oy, oz);
        return true;
    }

    return false;
}



void GLWidget::drawRay( const CRay& ray, vec4f color )
{
    vec3f e = ray.start + 10.0f*ray.direction;
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glBegin(GL_LINES);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color.ptr());
    glVertex3fv(ray.start.ptr());
    glVertex3fv(e.ptr());
    glEnd();
    glPopAttrib();
}

void GLWidget::drawLineSegment( vec3f s, vec3f e, vec4f color )
{  
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glBegin(GL_LINES);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color.ptr());
    glVertex3fv(s.ptr());
    glVertex3fv(e.ptr());
    glEnd();
    glPopAttrib();
}

vec3f GLWidget::mask(vec3f v, UITRANSFORMAXIS axis)
{
    if(axis == uiaFree)
        return v;
    else if(axis == uiaX)
        return vec3f(v.x, 0.0f, 0.0f);
    else if(axis == uiaY)
        return vec3f(0.0f, v.y, 0.0f);
    else if(axis == uiaZ)
        return vec3f(0.0f, 0.0f, v.z);
    else
        return v;
}

void GLWidget::maskMaterial( UITRANSFORMAXIS axis )
{
    static const GLfloat red[] = {1.0f, 0.0f, 0.0f, 1.0f};
    static const GLfloat green[] = {0.0f, 1.0f, 0.0f, 1.0f};
    static const GLfloat blue[] = {0.0f, 0.0f, 1.0f, 1.0f};
    static const GLfloat white[] = {1.0f, 1.0f, 1.0f, 1.0f};
    if((TheUITransform::Instance().axis == uiaFree)||((TheUITransform::Instance().axis != uiaFree)&&(TheUITransform::Instance().axis != axis)))
    {
        if(axis == uiaX)
            glColor3fv(red);
        //glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, red);
        else if(axis == uiaY)
            glColor3fv(green);
        //glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, green);
        else if(axis == uiaZ)
            glColor3fv(blue);
        //glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, blue);
    }
    else
        glColor3fv(white);
    //glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, white);
}

void GLWidget::resetTransformState()
{
    //TheUITransform::Instance().axis = uiaFree;
    TheUITransform::Instance().nStep = 0;
    TheUITransform::Instance().scale = vec3f(1.0f, 1.0f, 1.0f);
    TheUITransform::Instance().rotate.identity();
    TheUITransform::Instance().translate.zero();
    TheUITransform::Instance().mouseDown.zero();
    TheUITransform::Instance().mouseMove.zero();
}

void GLWidget::setMouseDragScale( int k )
{
    TheAppSettings::Instance().setSketch.mouseDragScale  = k;
}

void GLWidget::userInterfaceReady()
{
    //============================================================================
    //Set the number of threads
    emit sig_setParsipCellSize(static_cast<int>(DEFAULT_CELL_SIZE * CELL_SIZE_SCALE));
    //============================================================================
    //Select the first layer to start drawing right away
    m_layerManager.addLayer(NULL, "Layer 0");    
    m_layerManager.bumpRevisions();
    selectLayer(0);
    emit sig_showLayerManager(getModelLayerManager());

    //Set Threads Count
    setParsipThreadsCount(tbb::task_scheduler_init::automatic);
    emit sig_setParsipThreadsCount(TaskManager::getTaskManager()->getThreadCount());

    //Set default Materials
    for(int i=0; i<8; i++)
        m_materials[7 - i] = CMaterial::getMaterialFromList(i);
    emit sig_showColorRibbon(getModelColorRibbon());
    //switchToOrtho();
}

void GLWidget::setDisplayColorCodedMPUs( bool bEnable )
{
    TheAppSettings::Instance().setDisplay.bShowColorCodedMPUs = bEnable;
    //m_parsip.setColorCodedMPUs(bEnable);
}

void GLWidget::setDisplayGraph( bool bEnable )
{
    TheAppSettings::Instance().setDisplay.bShowGraph = bEnable;
    updateGL();
}

void GLWidget::setDisplayAnimCurves(bool bEnable)
{
    TheAppSettings::Instance().setDisplay.bShowAnimCurves = bEnable;
    updateGL();
}

void GLWidget::actEditTransformSkeleton( bool bEnable )
{
    m_bTransformSkeleton = bEnable;
}

void GLWidget::setParsipForceMC( bool bEnable )
{
    TheAppSettings::Instance().setParsip.bForceMC = bEnable;
    m_layerManager.bumpRevisions();
}

void GLWidget::actEditUndo()
{
    if(m_curUndoLevel >=0 && m_curUndoLevel < m_ctUndoLevels)
    {
        CSketchConfig* pUndoLevel = m_lstUndo[m_curUndoLevel];
        m_layerManager.loadScript(pUndoLevel);
        m_curUndoLevel--;

        emit sig_enableUndo(m_curUndoLevel >= 0);
        emit sig_enableRedo(m_ctUndoLevels - m_curUndoLevel > 1);

        actMeshPolygonize();
    }
}

void GLWidget::actEditRedo()
{
    if((m_curUndoLevel + 1) < m_ctUndoLevels)
    {
        m_curUndoLevel++;
        CSketchConfig* pUndoLevel = m_lstUndo[m_curUndoLevel];
        m_layerManager.loadScript(pUndoLevel);

        emit sig_enableUndo(m_curUndoLevel >= 0);
        emit sig_enableRedo(m_ctUndoLevels - m_curUndoLevel > 1);

        actMeshPolygonize();
    }
}

void GLWidget::actEditCopy()
{
    CLayer* active = m_layerManager.getActiveLayer();
    if(active == NULL) return;
    if(active->getBlob() == NULL) return;
    if(active->selCountItems() == 0) return;

    ConvertToBinaryTree* lpConverter = new ConvertToBinaryTree(active, false, false);

    CBlobNode* clonned = TheBlobNodeCloneFactory::Instance().CreateObject(active->selGetItem());
    clonned->setID(active->getIDDispenser().bump());
    if(lpConverter->run(active->selGetItem(), clonned) > 0)
    {        
        active->getBlob()->addChild(clonned);
        actMeshPolygonize(m_layerManager.getActiveLayerIndex());
        emit sig_showBlobTree(getModelBlobTree(m_layerManager.getActiveLayerIndex()));
    }
    else
        SAFE_DELETE(clonned);

    //Cleanup
    SAFE_DELETE(lpConverter);
}

void GLWidget::addUndoLevel()
{
    CSketchConfig* pUndoLevel = new CSketchConfig();
    m_layerManager.saveScript(m_layerManager.getActiveLayerIndex(), pUndoLevel);

    if(m_ctUndoLevels < MAX_UNDO_LEVEL)
    {
        m_ctUndoLevels++;
        m_curUndoLevel = m_ctUndoLevels-1;
        m_lstUndo[m_ctUndoLevels - 1] = pUndoLevel;
    }
    else
    {
        //Delete oldest entry
        CSketchConfig* pLevel0 = m_lstUndo[0];
        SAFE_DELETE(pLevel0);

        //Move all other entries one level down
        for(int i=1; i<MAX_UNDO_LEVEL; i++)
            m_lstUndo[i-1] = m_lstUndo[i];

        //Add latest
        m_lstUndo[MAX_UNDO_LEVEL-1] = pUndoLevel;
    }

    emit sig_enableUndo(true);
}

void GLWidget::resetUndoLevels()
{
    CSketchConfig* cfg;
    for(int i=0; i<m_ctUndoLevels;i++)
    {
        cfg = m_lstUndo[i];
        SAFE_DELETE(cfg);
    }
    m_lstUndo.resize(MAX_UNDO_LEVEL);

    m_ctUndoLevels = 0;
    m_curUndoLevel = 0;

    emit sig_enableRedo(false);
    emit sig_enableUndo(false);
}

void GLWidget::drawGraph()
{
    /*
 if(m_layerManager.countLayers() == 0) return;
 CParsipOptimized* parsip = m_layerManager[0]->getPolygonizer();
 if(parsip->countMPUs() == 0) return;

        FTPixmapFont font("C:\\Windows\\Fonts\\Calibri.ttf");
        if(font.Error()) return;

 vector<size_t> arrCoreThreadIDs;
 vector<double> arrCoreUtilizations;
 vector<int>	 arrCoreProcessedMPUs;


 int ctMPUs  = parsip->countMPUs();
 int ctCores = parsip->statsCoreUtilizations(arrCoreThreadIDs, arrCoreUtilizations);
 //Counting number of MPUs processed
 arrCoreProcessedMPUs.resize(ctCores);
 for(int i=0;i<ctCores;i++)
  arrCoreProcessedMPUs[i] = 0;

 double tsPolygonize = parsip->statsPolyTime() + parsip->statsSetupTime();
 double tsStart  = parsip->statsStartTime();
 //double tsEnd	= parsip->statsEndTime();
 float left, top;

 //Stats
 float sp = 10.0f;
 int ctPrims = 0;
 int ctOps = 0;
 for(size_t i=0;i<m_layerManager.countLayers();i++)
 {
  ctPrims += m_layerManager[i]->getCompactTree()->ctPrims;
  ctOps += m_layerManager[i]->getCompactTree()->ctOps;
 }

 DAnsiStr strText = printToAStr("Prims# %d, Ops# %d, Setup %.2f ,Poly %.2f, Total %.2f, fps = %d",
         ctPrims,
         ctOps,
         parsip->statsSetupTime(),
         parsip->statsPolyTime(),
         tsPolygonize,
         (int)(1000.0 / tsPolygonize));

 font.FaceSize(14);
 font.Render(strText.ptr(), -1, FTPoint(sp, m_scrDim.y - font.LineHeight()));



 float wTimeUnit = m_scrDim.x / static_cast<float>(tsPolygonize);
 Clampf(wTimeUnit, 1.0f, m_scrDim.x);

 float quadH = MATHMIN(font.LineHeight() + 2, (m_scrDim.y - font.LineHeight()) / static_cast<float>(ctCores));
 float quadW;

 //Save
 glPushAttrib(GL_ALL_ATTRIB_BITS);
 glPushMatrix();

 //Switch to orthographic view
 switchToOrtho();
 glEnable(GL_BLEND);
 glBlendFunc(GL_ONE, GL_ONE);

 glCullFace(GL_NONE);

 /////////////////////////////////////////
 CSIMDMPU* ampu = NULL;
 int iCore = 0;
 for(int iMPU=0; iMPU<ctMPUs; iMPU++)
 {
  ampu = parsip->getMPU(iMPU);
  if((ampu->statsProcessTime() > 0.0)&&(ampu->isReady()))
  {
   iCore = static_cast<int>(arrCoreThreadIDs.find_idx(ampu->getThreadId()));

   arrCoreProcessedMPUs[iCore]++;
   top   = iCore * (quadH + sp) + sp;
   left  = sp + (ampu->statsStartTime() - tsStart) * wTimeUnit;
   quadW = ampu->statsProcessTime() * wTimeUnit;

   glColor3f(0.0f, 1.0f, 0.0f);

   glBegin(GL_QUADS);
    glVertex2f(left, top);
    glVertex2f(left + quadW, top);
    glVertex2f(left + quadW, top + quadH);
    glVertex2f(left, top + quadH);
   glEnd();
  }
 }


 //////////////////////////////////////////
 float fontHeight = quadH - 2.0f;
 Clampf(fontHeight, 8.0f, 24.0f);


 font.FaceSize((int)fontHeight);

 for(int iCore=0; iCore<ctCores; iCore++)
 {
  strText = printToAStr("Core %d: Processed %d MPUs, Util %.2f",
         iCore+1,
         arrCoreProcessedMPUs[iCore],
         arrCoreUtilizations[iCore]);
  left = sp;
  top = iCore * (quadH + sp) + sp;


  font.Render(strText.ptr(), -1, FTPoint(left+1, top+1));
 }


 arrCoreThreadIDs.clear();
 arrCoreUtilizations.clear();
 arrCoreProcessedMPUs.clear();

 //Revert
 glCullFace(GL_BACK);
 glDisable(GL_BLEND);

 switchToProjection();

 //Restore
 glPopMatrix();
 glPopAttrib();
        */
}

void GLWidget::getFrustumPlane( float z, vec3f& bottomleft, vec3f& topRight )
{
    vec3f p = m_camera.getCoordinates();
    vec3f dir = m_camera.getCenter() - p;
    dir.normalize();

    float theta = FIELD_OF_VIEW_Y / 2.0f;

    //Point on center of plane
    //vec3f b  = z * dir;
    float hh = z * tan(DEGTORAD(theta));
    float ww = hh * (float) m_scrDim.x/ (float)m_scrDim.y;

    vec3f planeCenter = p + z*dir;

    vec3f w = dir.cross(vec3f(0.0f, 1.0f, 0.0f));
    w.normalize();

    bottomleft = planeCenter + w*vec3f(ww, hh, 0);
    topRight = planeCenter - w*vec3f(ww, hh, 0);
}

void GLWidget::actTestSetRuns( int value )
{
    TheAppSettings::Instance().setParsip.testRuns = value;
}

void GLWidget::actTestStart()
{
    //benchmarkSIMD();
    //actTestPerformIncreasingThreads();
    actTestPerformUtilizationTest();
    //actTestPerformStd();
}

void GLWidget::actTestPerformUtilizationTest()
{
    if(m_layerManager.countLayers() == 0) return;

    vector<DAnsiStr> strHeaders;
    DAnsiStr strOut = PS::FILESTRINGUTILS::ExtractFilePath(GetExePath()) + DAnsiStr("CoresUtilization.csv");

    PS::CPerfTest* perfCoreUtilizations = new PS::CPerfTest();
    perfCoreUtilizations->setOutputFileName(strOut, PS::CPerfTest::wtrCreateNew);
    int ctCores = TaskManager::getTaskManager()->getThreadCount();

    strHeaders.push_back("test#");
    for(int iCore=0;iCore<ctCores; iCore++)
        strHeaders.push_back(printToAStr("Core#%d", iCore+1));
    perfCoreUtilizations->setHeaders(strHeaders);

    //Run experiments with this amount of threads
    vector<size_t> arrThreads;
    vector<double> arrUtilization;


    for(int i=0; i<TheAppSettings::Instance().setParsip.testRuns; i++)
    {
        emit sig_setProgress(1, TheAppSettings::Instance().setParsip.testRuns, i+1);

        //Process Messages
        qApp->processEvents();

        actMeshPolygonize();


        //Cores Utilization
        strOut = printToAStr("%d,", i+1);
        m_layerManager[0]->getPolygonizer()->statsCoreUtilizations(arrThreads, arrUtilization);
        for(int j=0; j<ctCores;j++)
        {
            if(j<(int)arrUtilization.size())
                strOut += printToAStr("%.2f", arrUtilization[j] * 100.0f);
            else
                strOut += DAnsiStr("0");
            if(j<ctCores-1) strOut += DAnsiStr(",");
        }
        arrUtilization.clear();
        arrThreads.clear();
        perfCoreUtilizations->addToContent(strOut);
    }

    perfCoreUtilizations->writeCSV();
    SAFE_DELETE(perfCoreUtilizations);
}

//////////////////////////////////////////////////////////////////////////

void GLWidget::actTestPerformIncreasingThreads()
{
    if(m_layerManager.countLayers() == 0) return;

    vector<DAnsiStr> strHeaders;
    DAnsiStr strOut = PS::FILESTRINGUTILS::ExtractFilePath(GetExePath()) + DAnsiStr("CoresUtilization.csv");
    PS::CPerfTest* perfCoreUtilizations = new PS::CPerfTest();
    perfCoreUtilizations->setOutputFileName(strOut, PS::CPerfTest::wtrCreateNew);
    int ctCores = TaskManager::getTaskManager()->getThreadCount();

    strHeaders.push_back("test#");
    for(int iCore=0;iCore<ctCores; iCore++)
        strHeaders.push_back(printToAStr("Core#%d", iCore+1));
    perfCoreUtilizations->setHeaders(strHeaders);
    //***********************************************
    PS::CPerfTest* perftest = new PS::CPerfTest();
    perftest->setAppendMode(PS::CPerfTest::wtrCreateNew);
    strHeaders.clear();
    strHeaders.push_back(DAnsiStr("test#"));
    strHeaders.push_back(DAnsiStr("Cores Used"));
    strHeaders.push_back(DAnsiStr("Blob Primitives"));
    strHeaders.push_back(DAnsiStr("Blob Operators"));
    strHeaders.push_back(DAnsiStr("Poly Cell Size"));
    strHeaders.push_back(DAnsiStr("Total Setup Time"));
    strHeaders.push_back(DAnsiStr("Total Poly Time"));
    strHeaders.push_back(DAnsiStr("Total FieldEvaluations"));
    strHeaders.push_back(DAnsiStr("FEVPT"));

    strHeaders.push_back(DAnsiStr("MPUs Total"));
    strHeaders.push_back(DAnsiStr("MPUs Intersected"));
    strHeaders.push_back(DAnsiStr("MPUs ST"));
    strHeaders.push_back(DAnsiStr("MPUs MC"));
    strHeaders.push_back(DAnsiStr("Lastest MPU ProcessTime"));
    strHeaders.push_back(DAnsiStr("Lastest MPU IntersectedCells"));
    strHeaders.push_back(DAnsiStr("Lastest MPU FieldEvaluations"));
    strHeaders.push_back(DAnsiStr("Cells Total"));
    strHeaders.push_back(DAnsiStr("Cells Intersected"));
    strHeaders.push_back(DAnsiStr("Mesh Faces"));
    strHeaders.push_back(DAnsiStr("Mesh Vertices"));



    perftest->setHeaders(strHeaders);

    DAnsiStr str;

    int ctPrims = 0;
    int ctOperators = 0;
    for(size_t iLayer=0; iLayer < m_layerManager.countLayers(); iLayer++)
    {
        ctPrims += m_layerManager[iLayer]->getBlob()->recursive_CountPrimitives();
        ctOperators += m_layerManager[iLayer]->getBlob()->recursive_CountOperators();
    }

    double tsSetup, tsPoly;
    size_t ctV, ctT;

    vector<size_t> arrThreads;
    vector<double> arrUtilization;
    size_t testNumber;

    for(int iCore=1; iCore<=ctCores; iCore++)
    {
        emit sig_setProgress(1, ctCores, iCore);

        //Process Messages
        qApp->processEvents();

        //Set thread count
        setParsipThreadsCount(iCore);

        //Run experiments with this amount of threads
        for(int i=0; i<TheAppSettings::Instance().setParsip.testRuns; i++)
        {
            actMeshPolygonize();

            //Test Number
            testNumber = (iCore - 1) * TheAppSettings::Instance().setParsip.testRuns + i + 1;

            //Cores Utilization
            strOut = printToAStr("%d,", testNumber);

            CParsipOptimized* parsip = m_layerManager[0]->getPolygonizer();
            parsip->statsCoreUtilizations(arrThreads, arrUtilization);
            for(int j=0; j<ctCores;j++)
            {
                if(j<(int)arrUtilization.size())
                    strOut += printToAStr("%.2f", arrUtilization[j] * 100.0f);
                else
                    strOut += DAnsiStr("0");
                if(j<ctCores-1) strOut += DAnsiStr(",");
            }
            arrUtilization.clear();
            arrThreads.clear();
            perfCoreUtilizations->addToContent(strOut);

            //Polygonization
            tsSetup = parsip->statsSetupTime();
            tsPoly = parsip->statsPolyTime();
            parsip->statsMeshInfo(ctV, ctT);
            //m_parsip.getTimingStats(tsSetup, tsPoly);
            //m_parsip.getMeshStats(ctV, ctT);
            str = printToAStr("%d,%d,%d,%d,", testNumber, TaskManager::getTaskManager()->getThreadCount(), ctPrims, ctOperators);
            str += printToAStr("%.2f,%.2f,%.2f,", TheAppSettings::Instance().setParsip.cellSize, tsSetup, tsPoly);
            str += printToAStr("%d,%d,", parsip->statsTotalFieldEvals(), parsip->statsTotalFieldEvals() / ctT);
            str += printToAStr("%d,%d,%d,%d,",
                               parsip->countMPUs(),
                               parsip->statsIntersectedMPUs(),
                               0,
                               parsip->statsIntersectedMPUs());
            CSIMDMPU* ampu = parsip->statsLatestMPU();
            str += printToAStr("%.2f,%d,%d,",
                               ampu->statsProcessTime(),
                               ampu->statsIntersectedCells(),
                               ampu->statsFieldEvals());

            str += printToAStr("%d,%d,%d,%d",
                               parsip->statsTotalCellsInIntersectedMPUs(),
                               parsip->statsIntersectedCellsCount(),
                               ctT,
                               ctV);

            perftest->addToContent(str);
            //Sleep(1);
        }
    }

    perftest->writeCSV();
    perfCoreUtilizations->writeCSV();
    SAFE_DELETE(perftest);
    SAFE_DELETE(perfCoreUtilizations);
}

void GLWidget::actTestPerformStd()
{
    if(m_layerManager.countLayers() == 0) return;
    PS::CPerfTest* perftest = new PS::CPerfTest();
    perftest->setAppendMode(PS::CPerfTest::wtrCreateNew);

    vector<DAnsiStr> strHeaders;
    strHeaders.push_back(DAnsiStr("test#"));
    strHeaders.push_back(DAnsiStr("Cores Used"));
    strHeaders.push_back(DAnsiStr("Blob Primitives"));
    strHeaders.push_back(DAnsiStr("Blob Operators"));
    strHeaders.push_back(DAnsiStr("Poly Cell Size"));
    strHeaders.push_back(DAnsiStr("Total Setup Time"));
    strHeaders.push_back(DAnsiStr("Total Poly Time"));
    strHeaders.push_back(DAnsiStr("Total FieldEvaluations"));
    strHeaders.push_back(DAnsiStr("FEVPT"));

    strHeaders.push_back(DAnsiStr("MPUs Total"));
    strHeaders.push_back(DAnsiStr("MPUs Intersected"));
    strHeaders.push_back(DAnsiStr("MPUs ST"));
    strHeaders.push_back(DAnsiStr("MPUs MC"));
    strHeaders.push_back(DAnsiStr("Lastest MPU ProcessTime"));
    strHeaders.push_back(DAnsiStr("Lastest MPU IntersectedCells"));
    strHeaders.push_back(DAnsiStr("Lastest MPU FieldEvaluations"));
    strHeaders.push_back(DAnsiStr("Cells Total"));
    strHeaders.push_back(DAnsiStr("Cells Intersected"));
    strHeaders.push_back(DAnsiStr("Mesh Faces"));
    strHeaders.push_back(DAnsiStr("Mesh Vertices"));


    perftest->setHeaders(strHeaders);

    DAnsiStr str;

    int ctPrims = 0;
    int ctOperators = 0;
    for(size_t iLayer=0; iLayer < m_layerManager.countLayers(); iLayer++)
    {
        ctPrims += m_layerManager[iLayer]->getBlob()->recursive_CountPrimitives();
        ctOperators += m_layerManager[iLayer]->getBlob()->recursive_CountOperators();
    }

    double tsSetup, tsPoly;
    size_t ctV, ctT;
    for(int i=0; i<TheAppSettings::Instance().setParsip.testRuns; i++)
    {
        emit sig_setProgress(0, TheAppSettings::Instance().setParsip.testRuns, i+1);

        //Process Messages
        qApp->processEvents();

        actMeshPolygonize();

        CParsipOptimized* parsip = m_layerManager[0]->getPolygonizer();

        tsSetup = parsip->statsSetupTime();
        tsPoly = parsip->statsPolyTime();

        parsip->statsMeshInfo(ctV, ctT);
        str = printToAStr("%d,%d,%d,%d,", i+1, TaskManager::getTaskManager()->getThreadCount(), ctPrims, ctOperators);
        str += printToAStr("%.2f,%.2f,%.2f,", TheAppSettings::Instance().setParsip.cellSize, tsSetup, tsPoly);
        str += printToAStr("%d,%d,", parsip->statsTotalFieldEvals(), parsip->statsTotalFieldEvals() / ctT);
        str += printToAStr("%d,%d,%d,%d,",
                           parsip->countMPUs(),
                           parsip->statsIntersectedMPUs(),
                           0,
                           parsip->statsIntersectedMPUs());
        CSIMDMPU* ampu = parsip->statsLatestMPU();
        str += printToAStr("%.2f,%d,%d,",
                           ampu->statsProcessTime(),
                           ampu->statsIntersectedCells(),
                           ampu->statsFieldEvals());
        str += printToAStr("%d,%d,%d,%d",
                           parsip->statsTotalCellsInIntersectedMPUs(),
                           parsip->statsIntersectedCellsCount(),
                           ctT,
                           ctV);

        perftest->addToContent(str);
    }

    perftest->writeCSV();
    SAFE_DELETE(perftest);
}

void GLWidget::actFileModelPiza()
{
    actFileClose();

    DlgPizaModelBuilder* lpDlgPiza = new DlgPizaModelBuilder(this);
    lpDlgPiza->setModal(true);
    lpDlgPiza->setValues(4, 8, 6.0f, 6.0f);
    connect(lpDlgPiza, SIGNAL(sig_preview(int,int,int,int, float,float)), this, SLOT(actFileModelPiza(int, int, int, int, float, float)));
    if(lpDlgPiza->exec() == QDialog::Accepted)
    {
        int levels, pillars;
        int xTowers, yTowers;
        float radius, height;
        lpDlgPiza->getValues(levels, pillars,
                             xTowers, yTowers,
                             radius, height);

        //Global Union
        CUnion* root = new CUnion();

        //Create a tower
        CBlobNode* tower = Shapes::createPiza(levels, pillars, radius, height);
        tower->getTransform().setTranslate(vec3f(0.0f, -height, 0.0f));
        root->addChild(tower);

        //Instance root multiple times           
        for(int i=0; i < xTowers; i++)
            for(int j=0; j < yTowers; j++)
        {
            if(!(i==0 && j==0))
            {
                float x = 3 * radius * i;
                float z = 3 * radius * j;

                //Instance
                CInstance* inst = new CInstance(tower);
                inst->getTransform().setTranslate(vec3f(x, 0, z));
                root->addChild(inst);
            }
        }

        m_layerManager.addLayer(root, "PIZA");
        m_layerManager.setActiveLayer(0);
        m_layerManager.getLayer(0)->reassignBlobNodeIDs();
        m_layerManager.bumpRevisions();
        emit sig_showLayerManager(getModelLayerManager());
        emit sig_showBlobTree(getModelBlobTree(0));

        //Convert to Binary Tree
        QMessageBox msg;
        msg.setText("Convert to binary tree? [In case of unary node it will be padded with NULL primitive]");
        msg.setInformativeText("Convert to Binary Tree? [PADWITHNULL, NO SPLIT WHEN ALL CHILDREN ARE PRIMS]");
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        if(msg.exec() == QMessageBox::Yes)
        {
            m_layerManager.getActiveLayer()->selRemoveItem();
            m_layerManager.getActiveLayer()->flattenTransformations();
            m_layerManager.getActiveLayer()->convertToBinaryTree(true, false);
            emit sig_showLayerManager(getModelLayerManager());
            emit sig_showBlobTree(getModelBlobTree(0));
        }
    }

    SAFE_DELETE(lpDlgPiza);
}

void GLWidget::actFileModelMedusa()
{
    actFileClose();
    //Select the first layer to start drawing right away
    //Galin Medusa Model
    MEDUSABLENDTYPE method = mbtOneFastQuadricPointSet;

    CBlend* lpMedusa = new CBlend();    
    /*
    static CBlobNode* Medusa_Hair(MEDUSABLENDTYPE blend = mbtOneSumBlend);
    static CBlobNode* Medusa_Head(MEDUSABLENDTYPE blend = mbtOneSumBlend);
    static CBlobNode* Medusa_Neck(MEDUSABLENDTYPE blend = mbtOneSumBlend );
    static CBlobNode* Medusa_Body(MEDUSABLENDTYPE blend = mbtOneSumBlend);
    static CBlobNode* Medusa_Breast(MEDUSABLENDTYPE blend = mbtOneSumBlend);
    static CBlobNode* Medusa_Tail(MEDUSABLENDTYPE blend = mbtOneSumBlend);
    static CBlobNode* Medusa_LeftHand(MEDUSABLENDTYPE blend = mbtOneSumBlend);
    */
    QMessageBox msg1;
    msg1.setText("Do you want medusa with hair [complete] or without hair? [hair:6510, head:640, neck:20, body:840, chest:40, tail:810, lefthand:570]");
    msg1.setInformativeText("Medusa with hair?");
    msg1.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    if(msg1.exec() == QMessageBox::Yes)
    {
        lpMedusa->addChild(GalinMedusaGenerator::Medusa_Hair(method));
        lpMedusa->addChild(GalinMedusaGenerator::Medusa_Head(method));
        lpMedusa->addChild(GalinMedusaGenerator::Medusa_Neck(method));
        lpMedusa->addChild(GalinMedusaGenerator::Medusa_Body(method));
        lpMedusa->addChild(GalinMedusaGenerator::Medusa_Breast(method));
        lpMedusa->addChild(GalinMedusaGenerator::Medusa_Tail(method));
        lpMedusa->addChild(GalinMedusaGenerator::Medusa_LeftHand(method));
    }
    else
    {
        lpMedusa->addChild(GalinMedusaGenerator::Medusa_Head(method));
        lpMedusa->addChild(GalinMedusaGenerator::Medusa_Neck(method));
        lpMedusa->addChild(GalinMedusaGenerator::Medusa_Body(method));
        lpMedusa->addChild(GalinMedusaGenerator::Medusa_Breast(method));
        lpMedusa->addChild(GalinMedusaGenerator::Medusa_Tail(method));
        lpMedusa->addChild(GalinMedusaGenerator::Medusa_LeftHand(method));
    }

    //Medusa
    m_layerManager.addLayer(lpMedusa, "Medusa");
    m_layerManager.setActiveLayer(0);
    m_layerManager.getLayer(0)->reassignBlobNodeIDs();
    m_layerManager.bumpRevisions();
    emit sig_showLayerManager(getModelLayerManager());
    emit sig_showBlobTree(getModelBlobTree(0));

    //Convert to Binary Tree
    QMessageBox msg2;
    msg2.setText("Convert to binary tree? [PADWITHNULL, NO SPLIT WHEN ALL CHILDREN ARE PRIMS]");
    msg2.setInformativeText("Convert to Binary Tree?");
    msg2.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    if(msg2.exec() == QMessageBox::Yes)
    {
        m_layerManager.getActiveLayer()->selRemoveItem();
        m_layerManager.getActiveLayer()->flattenTransformations();
        m_layerManager.getActiveLayer()->convertToBinaryTree(true, false);
        emit sig_showLayerManager(getModelLayerManager());
        emit sig_showBlobTree(getModelBlobTree(0));
    }
}

void GLWidget::switchToOrtho()
{
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0.0, m_scrDim.x, 0.0, m_scrDim.y);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void GLWidget::switchToProjection()
{
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void GLWidget::drawString( const char* str, float x, float y )
{
    /*
 FTPixmapFont font("C:\\Windows\\Fonts\\Calibri.ttf");

 FTPoint pt(x, y, 0.0f);
 if(!font.Error())
 {
  font.FaceSize(32);
  font.Render(str, -1, pt);
 }
        */
}

bool GLWidget::actNetRecvCommand( int idxMember, QString strMsg )
{
    //Find sender
    CMember* peer = CDesignNet::GetDesignNet()->getMember(idxMember);
    if(peer == NULL)
    {
        emit sig_addNetLog(QString("Unable to find sender of the last message."));
        return false;
    }

    //Decode Received message
    SKETCHCMDPACKET rxMsg;
    if(!CSketchNetCommandTranslator::GetCmdTranslator()->translateStrToPacket(strMsg, rxMsg))
    {
        emit sig_addNetLog(QString("Unable to understand message."));
        return false;
    }

    //Received a valid message from a known source
    //Activate source in the peers list
    peer->incrRecv();
    peer->activate();


    //Find Active Layer
    CLayer* active = m_layerManager.getActiveLayer();
    if(active == NULL)
    {
        emit sig_addNetLog(QString("Unable to find an active layer."));
        return false;
    }


    //Add Undo Level
    addUndoLevel();

    //If it is an Ack forward it to member
    if(rxMsg.cmd == cmdAck)
    {
        return (peer->recvAck(rxMsg) == ackSuccess);
    }
    else
    {
        SKETCHACK result = ackSuccess;
        switch(rxMsg.cmd)
        {
        case(cmdAdd):
        {
            CBlobNode* prim = addBlobPrimitive(rxMsg.nodeType, vec3f(0.0f, 0.0f, 0.0f), rxMsg.blobnodeID,false);
            if(prim != NULL)
            {
                prim->setMaterial(ColorToMaterial(rxMsg.param));
            }
        }
            break;
        case(cmdOperator):
        {
            active->selRemoveItem();
            CBlobNode* child1 = active->findNodeByID(rxMsg.leftChild);
            CBlobNode* child2 = active->findNodeByID(rxMsg.rightChild);
            if((child1 == NULL)||(child2 == NULL))
                result = ackIDNotFound;
            else
            {
                active->selAddItem(child1);
                active->selAddItem(child2);
                addBlobOperator(rxMsg.nodeType, rxMsg.blobnodeID, false);
            }
        }
            break;
        case(cmdMove):
        {
            CBlobNode* child1 = active->findNodeByID(rxMsg.blobnodeID);
            if(child1)
            {
                if(child1->getLock().acquire())
                {
                    child1->getTransform().addTranslate(rxMsg.param.xyz());
                    child1->getLock().release();
                }
                else
                    result = ackIDLocked;
            }
            else result = ackIDNotFound;
        }
            break;

        case(cmdScale):
        {
            CBlobNode* child1 = active->findNodeByID(rxMsg.blobnodeID);
            if(child1)
            {
                if(child1->getLock().acquire())
                {
                    child1->getTransform().addScale(rxMsg.param.xyz());
                    child1->getLock().release();
                }
                else
                    result = ackIDLocked;
            }
            else result = ackIDNotFound;
        }
            break;
        case(cmdRotate):
        {
            CBlobNode* child1 = active->findNodeByID(rxMsg.blobnodeID);
            if(child1)
            {
                if(child1->getLock().acquire())
                {
                    child1->getTransform().addRotate(CQuaternion(rxMsg.param));
                    child1->getLock().release();
                }
                else
                    result = ackIDLocked;
            }
            else result = ackIDNotFound;
        }
            break;
        case(cmdLock):
        {
            CBlobNode* child1 = active->findNodeByID(rxMsg.blobnodeID);
            if(child1)
            {
                if(!child1->getLock().acquire(DAnsiStr(peer->m_strAddress.toLatin1().data())))
                    result = ackIDLocked;
            }
        }
            break;

        case(cmdUnlock):
        {
            CBlobNode* child1 = active->findNodeByID(rxMsg.blobnodeID);
            if(child1)
            {
                if((child1->getLock().isLocked())&&
                        (child1->getLock().getOwner() == DAnsiStr(peer->m_strAddress.toLatin1().data())))
                    child1->getLock().release();
                else
                    result = ackIDNotLocked;
            }
        }
            break;
        case(cmdDelete):
        {
            CBlobNode* child1 = active->findNodeByID(rxMsg.blobnodeID);
            if(child1)
            {
                if(child1->getLock().acquire())
                {
                    active->selRemoveItem(-1);
                    active->selAddItem(child1);
                    actEditDelete();
                }
                else
                    result = ackIDLocked;
            }
            else
                result = ackIDNotFound;
        }
            break;
        case(cmdSet):
        {
            CBlobNode* child1 = active->findNodeByID(rxMsg.blobnodeID);
            if(child1)
            {
                if(child1->getLock().acquire())
                {
                    active->selRemoveItem(-1);

                    //Apply Replacement if needed
                    if(child1->getNodeType() != rxMsg.nodeType)
                    {
                        CBlobNode* replacement = TheBlobNodeFactoryIndex::Instance().CreateObject(rxMsg.nodeType);

                        //Run Transform Command
                        PS::BLOBTREE::CmdBlobTreeParams param;
                        param.lpReplacementNode = replacement;
                        param.lpOutParent = NULL;
                        param.depth = 0;
                        param.idxChild = -1;

                        active->recursive_ExecuteCmdBlobtreeNode(active->getBlob(), child1, cbtTransformOperator, &param);

                        emit sig_showBlobTree(getModelBlobTree(m_layerManager.getActiveLayerIndex()));
                    }
                    else
                        child1->getLock().release();
                }
                else
                    result = ackIDLocked;
            }
            else
                result = ackIDNotFound;

        }
            break;
        }

        if(result == ackSuccess)
        {
            active->getTracker().bump();
            actMeshPolygonize(m_layerManager.getActiveLayerIndex());
        }

        return actNetSendAck(result, idxMember, rxMsg.msgID);
    }

    return false;
}


bool GLWidget::actNetSendAck( SKETCHACK ack, int idxMember, int msgID )
{
    CMember* peer = CDesignNet::GetDesignNet()->getMember(idxMember);
    if(peer == NULL) return false;

    SKETCHCMDPACKET txMsg;
    txMsg.cmd = cmdAck;
    txMsg.ack = ack;
    txMsg.msgID = msgID;

    QString strOutput;
    PS::CSketchNetCommandTranslator::GetCmdTranslator()->translatePacketToStr(txMsg, strOutput);
    return peer->sendText(strOutput);
}

bool GLWidget::actNetSendCommand( SKETCHCMD command, CBlobNode* node, vec4f param )
{
    if(CDesignNet::GetDesignNet()->countMembers() == 0) return false;

    SKETCHCMDPACKET txMsg;

    //Message ID field will be filled before sending the packet
    //by member
    txMsg.cmd	 = command;
    txMsg.param  = param;

    if(node)
    {
        txMsg.blobnodeID = node->getID();
        if(command == cmdAdd)
        {            
            txMsg.nodeType = node->getNodeType();
            txMsg.param = node->getMaterial().diffused;
        }
        else if(command == cmdOperator)
        {
            txMsg.nodeType = node->getNodeType();
            txMsg.leftChild  = -1;
            txMsg.rightChild = -1;
            if(node->countChildren() == 1)
                txMsg.leftChild = node->getChild(0)->getID();
            else if(node->countChildren() >= 2)
            {
                txMsg.leftChild = node->getChild(0)->getID();
                txMsg.rightChild= node->getChild(1)->getID();
            }
        }
    }

    if(CDesignNet::GetDesignNet()->sendPacketToMember(txMsg))
    {
        emit sig_addNetLog(QString("Command Sent to peers"));
        return true;
    }
    else
        return false;
}

//////////////////////////////////////////////////////////////////////////
void GLWidget::actFileExportMesh()
{
    if(m_layerManager.countLayers() == 0) return;
    DAnsiStr strFilePath = GetExePath();
    QString strFileName = QFileDialog::getSaveFileName(this, tr("Export Mesh"),
                                                       strFilePath.ptr(),
                                                       tr("Obj File(*.obj)"));
    //lstActions
    QString strActionsFN = strFileName + QString(".acts");
    CBlobTreeToActions* toActions = new CBlobTreeToActions(m_layerManager[0]);
    if(toActions->convert(strActionsFN.toLatin1().data()))
    {
        selectLayer(0);
    }
    SAFE_DELETE(toActions);

    //Export mesh
    CMeshVV output;
    bool bres = m_layerManager[0]->getPolygonizer()->exportMesh(&output);
    bres &= output.saveOBJ(DAnsiStr(strFileName.toLatin1().data()));

    //Save Mesh
    if(!bres)
    {
        ReportError("Unable to save!");
        FlushAllErrors();
    }

}

void GLWidget::actAnimDrawGuide()
{
    CLayer* active = m_layerManager.getActiveLayer();
    if(active == NULL)
        return;
    CBlobNode* root = active->selGetItem();
    if(root == NULL)
        return;
    if(CAnimManagerSingleton::Instance().getObject(root) != NULL)
    {
        ReportError("Selected model already exists in path animation.");
        FlushAllErrors();
        return;
    }

    CAnimManagerSingleton::Instance().addModel(root);
    m_uiMode = uimAnimation;
}

void GLWidget::actAnimStart()
{
    m_animTime = 0.0f;
    if(CAnimManagerSingleton::Instance().countObjects() > 0)
        m_timer->start();
}

void GLWidget::actAnimStop()
{
    m_timer->stop();
}

void GLWidget::advanceAnimation()
{  
    if(CAnimManagerSingleton::Instance().countObjects() == 0) return;

    CLayer* active = m_layerManager.getActiveLayer();
    if(active == NULL) return;

    //Advance animation
    CAnimManagerSingleton::Instance().advanceAnimation(m_animTime);

    //Polygonize
    active->getTracker().bump();
    actMeshPolygonize();

    //Increment
    float delta = ANIMATION_FRAME_TIME*m_animSpeed;
    m_animTime += delta;
    if(m_animTime >= 1.0f)
        actAnimStop();

    //actAnimStop();
}

void GLWidget::actEditFieldEditor()
{
    m_dlgFieldEditor->setModal(true);
    m_dlgFieldEditor->show();
}

void GLWidget::actEditConvertToBinaryTree()
{
    if(m_layerManager.getActiveLayer() == NULL)
        return;

    m_layerManager.getActiveLayer()->selRemoveItem();
    if(m_layerManager.getActiveLayer()->convertToBinaryTree(true, true) > 0)
    {
        QMessageBox msgBox;
        msgBox.setText("Conversion completed successfully. [PADWITHNULL, ALWAYSSPLIT]");
        msgBox.exec();
    }

    //Re-polygonize    
    m_layerManager.getActiveLayer()->getTracker().bump();    
    emit sig_showBlobTree(getModelBlobTree(m_layerManager.getActiveLayerIndex()));
}

void GLWidget::actEditProbe(bool bEnable)
{
    m_bProbing = bEnable;
    m_probePoint.zero();
    m_probeValue = 0.0f;

    actEditTranslate();
    updateProbe();
    updateGL();
}

void GLWidget::actEditBlobTreeStats()
{
    CLayer* active = m_layerManager.getActiveLayer();
    if(active && active->getBlob())
    {
        int ctPrims = active->getBlob()->recursive_CountPrimitives();
        int ctOps = active->getBlob()->recursive_CountOperators();
        int depth = active->getBlob()->recursive_Depth();

        DAnsiStr strMsg = printToAStr("Prims# %d, Ops# %d, and depth = %d exist in layer %s.",
                                      ctPrims, ctOps, depth, active->getGroupName().cptr());

        QMessageBox msgBox;
        msgBox.setText(strMsg.cptr());
        msgBox.exec();
    }
}

void GLWidget::actEditAssignIDs()
{
    CLayer* active = m_layerManager.getActiveLayer();
    if(active && active->getBlob())
    {
        active->reassignBlobNodeIDs();

        //Repolygonize and show
        active->getTracker().bump();
        emit sig_showBlobTree(getModelBlobTree(m_layerManager.getActiveLayerIndex()));
    }
}

void GLWidget::actEditMakeRoot()
{
    CLayer* active = m_layerManager.getActiveLayer();
    if(active && active->getBlob() && (active->selCountItems() > 0))
    {
        CBlobNode* lpParent = NULL;
        CBlobNode* lpQuery = active->selGetItem(0);
        if(active->actBlobFindParent(lpQuery, lpParent))
        {
            lpParent->detachChild(lpQuery);
            SAFE_DELETE(lpParent);
            active->setBlob(lpQuery);

            //Repolygonize and show
            active->getTracker().bump();            
            emit sig_showBlobTree(getModelBlobTree(m_layerManager.getActiveLayerIndex()));

            //actMeshPolygonize();
        }
    }

}

void GLWidget::updateProbe()
{
    CLayer* active = m_layerManager.getActiveLayer();
    if(active == NULL) return;

    CBlobNode* root = active->selGetItem();
    if((root == NULL)||(active->getCompactBlobTree() == NULL))
        return;


    bool bIsOp = root->isOperator();
    int id = active->getCompactBlobTree()->getID(bIsOp, root->getID());
    m_probeValue = active->getCompactBlobTree()->fieldAtNode(bIsOp, id, vec4f(m_probePoint, 0.0f));

    //Compute P0: closest point to fp2
    if(m_probeValue > FIELD_VALUE_EPSILON)
    {
        vec3f grad = active->getCompactBlobTree()->gradientAtNode(bIsOp, id, vec4f(m_probePoint, 0.0f), m_probeValue, NORMAL_DELTA).xyz();

        int dir = (m_probeValue < ISO_VALUE)?1:-1;
        float step = ISO_DISTANCE;

        //Uses shrink-wrap method to converge to surface
        //float d = (ISO_VALUE - m_probeValue) / (grad.dot(grad));
        //m_probeProjected = m_probePoint + (grad * d);
        int ctSteps = 0;

        m_probeProjected = m_probePoint;
        float f = m_probeValue;
        while(!FLOAT_EQ(f, ISO_VALUE, FIELD_VALUE_EPSILON))
        {
            m_probeProjected += step * dir * grad;
            f = active->getCompactBlobTree()->fieldAtNode(bIsOp, id , vec4f(m_probeProjected, 0.0f));
            int dir2 = (f < ISO_VALUE)?1:-1;
            if(dir != dir2)
                step *= 0.5f;
            dir = dir2;
            ctSteps++;
        }
    }
    else
        m_probeProjected.zero();

    //Update
    emit sig_showPrimitiveProperty(getModelPrimitiveProperty(root));
    updateGL();
}

void GLWidget::actAnimSetStartLoc()
{
    CLayer* active = m_layerManager.getActiveLayer();
    if(active == NULL) return;

    CAnimObject* obj = CAnimManagerSingleton::Instance().getObject(active->selGetItem(0));
    if(obj)
    {
        obj->gotoStart();

        active->getTracker().bump();
        actMeshPolygonize(m_layerManager.getActiveLayerIndex());
    }
}

void GLWidget::actAnimSetEndLoc()
{
    CLayer* active = m_layerManager.getActiveLayer();
    if(active == NULL) return;

    CAnimObject* obj = CAnimManagerSingleton::Instance().getObject(active->selGetItem(0));
    if(obj)
    {
        obj->gotoEnd();

        active->getTracker().bump();
        actMeshPolygonize(m_layerManager.getActiveLayerIndex());
    }
}

void GLWidget::actAnimSetStartScale()
{
    CLayer* active = m_layerManager.getActiveLayer();
    if(active == NULL) return;

    CAnimObject* obj = CAnimManagerSingleton::Instance().getObject(active->selGetItem(0));
    if(obj)
    {
        vec3f s = active->selGetItem(0)->getTransform().getScale();
        obj->startVal = vec4f(s.x, s.y, s.z, 1.0f);
        obj->bScale = true;

        QMessageBox msgBox;
        msgBox.setText("Scale factor for start location set successfully.");
        msgBox.exec();
    }
}

void GLWidget::actAnimSetEndScale()
{
    CLayer* active = m_layerManager.getActiveLayer();
    if(active == NULL) return;

    CAnimObject* obj = CAnimManagerSingleton::Instance().getObject(active->selGetItem(0));
    if(obj)
    {
        vec3f s = active->selGetItem(0)->getTransform().getScale();
        obj->endVal = vec4f(s.x, s.y, s.z, 1.0f);
        obj->bScale = true;

        QMessageBox msgBox;
        msgBox.setText("Scale factor for end location set successfully.");
        msgBox.exec();
    }
}

void GLWidget::setAnimationSpeed( int speed )
{
    m_animSpeed = static_cast<float>(speed);
}

void GLWidget::actAnimRemoveAll()
{
    CAnimManagerSingleton::Instance().removeAll();
    updateGL();
}



