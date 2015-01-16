#include "PointCloudFitting.h"

#include "distTransform.h"
#include <iostream>
#include <assert.h>
#include <float.h>

#include <pcl/point_types.h>
#include <pcl/common/centroid.h>
#include <pcl/io/pcd_io.h>
#include <Eigen/SVD>
#include <Eigen/Eigen>
#include <bot_core/rotations.h>
#include <pcl/filters/voxel_grid.h>

using namespace std;
using namespace pcl;
using namespace Eigen;

#define SQ(x) ((x)*(x))

namespace PointCloudFitting {

Affine3f pointCloutFit(PointCloud<PointXYZRGB>::ConstPtr modelcloud, 
                         PointCloud<PointXYZRGB>::ConstPtr cloud,
                         std::vector<float>& res_range)
{ 
  vector<Vector3f> modelcloudV(modelcloud->size());
  for(int i=0;i<modelcloud->size();i++) {
    modelcloudV[i][0] = modelcloud->at(i).x;
    modelcloudV[i][1] = modelcloud->at(i).y;
    modelcloudV[i][2] = modelcloud->at(i).z;
  }

  vector<Vector3f> cloudV(cloud->size());
  for(int i=0;i<cloud->size();i++) {
    cloudV[i][0] = cloud->at(i).x;
    cloudV[i][1] = cloud->at(i).y;
    cloudV[i][2] = cloud->at(i).z;
  }

  return align_coarse_to_fine(modelcloudV, cloudV, res_range);
 
}

static Matrix3f ypr2rot(Vector3f ypr){
  double rpy[]={ypr[2],ypr[1],ypr[0]};
  double q[4];
  double mat[9];
  bot_roll_pitch_yaw_to_quat(rpy,q);
  int rc=bot_quat_to_matrix(q,mat);
  Matrix3d mat2(mat);
  for(int j=0;j<3;j++){
    for(int i=0;i<3;i++){
      mat2(j,i) = mat[j*3+i];
    }
  }
  return mat2.cast<float>();
}

static Vector3f rot2ypr(Matrix3f mat){
  Matrix3d mat2 = mat.transpose().cast<double>(); //convert from col-major to row-major
  double* mat3 = mat2.data();
  double q[4];
  double rpy[3];
  bot_matrix_to_quat(mat3,q);

  // in some cases, q is NaN.  Try to fix it. HACK //TODO do better fix
  // ususally caused by numbers near 0 and 1
  if(std::isnan(q[0])){
    cout << "***** Warning quat is NaN, trying to fix.  Matrix:\n"<< mat << endl;
    for(int i=0;i<9;i++) mat3[i] = round(mat3[i]);
    bot_matrix_to_quat(mat3,q);
    if(std::isnan(q[0])) cout << "Couldn't fix\n";
    else            cout << "Fix seemed to work.\n";
  }
  
  bot_quat_to_roll_pitch_yaw(q,rpy);
  return Vector3f(rpy[2],rpy[1],rpy[0]);
}


void align_pts_3d(const vector<Vector3f>& pts_ref, const vector<Vector3f>& pts_cur,
                  Matrix3f& R, Vector3f& T){
  ASSERT_PI(pts_ref.size()==pts_cur.size());

  // compute mean
  Vector3f avg_ref(0,0,0), avg_cur(0,0,0);
  int N=pts_ref.size();
  for(int i=0;i<N;i++){
    avg_ref+=pts_ref[i];
    avg_cur+=pts_cur[i];
  }
  avg_ref/=N; avg_cur/=N;
  //cout << "avg: " << avg_ref.transpose() << " " <<  avg_cur.transpose() << endl;

  // sub mean
  vector<Vector3f> p_ref(N), p_cur(N);
  for(int i=0;i<N;i++){
    p_ref[i] = pts_ref[i]-avg_ref;
    p_cur[i] = pts_cur[i]-avg_cur;
    
  }

  // trans mat
  Affine3f T_ref = Affine3f::Identity();
  Affine3f T_cur = Affine3f::Identity();
  T_ref.translation() = -avg_ref; 
  T_cur.translation() = -avg_cur;

  // svd
  Matrix3f svdmat = Matrix3f::Zero();
  for(int i=0;i<N;i++){
    for(int j=0;j<N;j++){
      svdmat +=  p_ref[i]*p_cur[i].transpose();
    }
  }
  JacobiSVD<Matrix3f> svd(svdmat, ComputeFullU | ComputeFullV);
  Matrix3f u = svd.matrixU();
  Matrix3f v = svd.matrixV();
  //cout << "svdmat\n" << svdmat << endl;
  //cout << "u\n" << u << endl; 
  //cout << "v\n" << v << endl;

  Matrix3f R3 = u*v.transpose();
  Affine3f R4 = Affine3f::Identity();
  R4.linear() = R3;
  Affine3f P = T_ref.inverse()*R4*T_cur;
  
  // results
  R = P.linear();
  T = P.translation();

}

void create_voxels(const vector<Vector3f>& pts, float res, float padding,
                   Cube<float>& vol, Affine3f& world_to_vol)
{
  Vector3f pt_min, pt_max;
  pt_min = pt_max = pts[0];
  for(int i=0;i<pts.size();i++){
    pt_min = pt_min.cwiseMin(pts[i]);
    pt_max = pt_max.cwiseMax(pts[i]);
  }

  // create world to vol
  world_to_vol = Affine3f::Identity();
  world_to_vol.translation() = Vector3f(-padding,-padding,-padding);
  Affine3f scale = Affine3f::Identity();
  scale(0,0) = res;
  scale(1,1) = res;
  scale(2,2) = res;
  world_to_vol = scale*world_to_vol;
  Affine3f trans = Affine3f::Identity();
  trans.translation() = pt_min;
  world_to_vol = trans*world_to_vol;
  world_to_vol = world_to_vol.inverse();
  
  // alloc vol
  Vector3f sizef = (pt_max-pt_min)/res+2*Vector3f(padding,padding,padding)+Vector3f(1,1,1);
  Vector3i size(ceil(sizef[0]),  ceil(sizef[1]), ceil(sizef[2]));
  vol = Cube<float>(size[0], size[1], size[2]);
  //cout << size.transpose( ) << endl;
  //cout << pt_max.transpose( ) << " " << pt_min.transpose() << endl;


  // populate vol
  for(int i=0;i<pts.size();i++){
    Vector4f pts4(pts[i][0],pts[i][1],pts[i][2],1.0);
    Vector4f vol4 = world_to_vol*pts4;
    Vector3i voli(round(vol4[0]),round(vol4[1]),round(vol4[2]));
    //cout << i << " " << voli.transpose() << endl;
    //cout << i << " " << size.transpose() << endl;
    ASSERT_PI(voli[0]>=0 && voli[1]>=0 && voli[2]>=0);
    ASSERT_PI(voli[0]<size[0] && voli[1]<size[1] && voli[2]<size[2]);
    vol(voli[0],voli[1],voli[2]) = 1;
  }
}

void point_pairs_from_dist_inds(const Cube<int>& dist_inds, const vector<Vector3f>& pts, 
                                Matrix3f R, Vector3f T,
                                vector<Vector3f>& p0, vector<Vector3f>& p1)
{
  p0.clear();
  p1.clear();

  for(int i=0;i<pts.size();i++){
    Vector3f pt_cur = R*pts[i]+T; 
    Vector3i pt_int(round(pt_cur[0]), round(pt_cur[1]), round(pt_cur[2]));
    bool good=true;
    for(int j=0;j<3;j++) if(pt_int[j]<0||pt_int[j]>=dist_inds.size[j]) good=false;

    if(good){
      int model_ind = dist_inds(pt_int[0],pt_int[1], pt_int[2]);
      Vector3f modelPos = dist_inds.index(model_ind).cast<float>();
      p0.push_back(modelPos);
      p1.push_back(pt_cur);
    }
  }
}
              
Affine3f optimize_pose_with_directions(const Cube<int>& dist_inds, const vector<Vector3f>& pts, Affine3f pose_init){
  int max_iter=100;

  Affine3f P = pose_init;

  double err_prev = DBL_MAX;
  vector<Vector3f> p0,p1;
  for(int i=0;i<max_iter;i++){
    point_pairs_from_dist_inds(dist_inds, pts, P.linear(), P.translation(), p0, p1);
    Matrix3f R_new;
    Vector3f T_new;
    align_pts_3d(p0,p1,R_new,T_new);
    Affine3f P_new;
    P_new.linear() = R_new;
    P_new.translation() = T_new;
    P=P_new*P;

    double err_cur=0;
    for(int j=0;j<p0.size();j++){
      Vector3f d = (p1[j]-p0[j]).cwiseAbs();
      err_cur += SQ(d[0])+SQ(d[1])+SQ(d[2]);
    }
    if(abs(err_cur-err_prev)<1e-6) break;
    cout << err_cur << endl;
    err_prev = err_cur;
  }

  return P;
    
}

void decimate_points(const vector<Vector3f>& in, vector<Vector3f>& out, float res)
{
  // copy to cloud
  PointCloud<PointXYZRGB>::Ptr cloud(new PointCloud<PointXYZRGB>);
  cloud->points.resize(in.size());
  for(int i=0;i<in.size();i++) {
    cloud->points[i].x = in[i][0];
    cloud->points[i].y = in[i][1];
    cloud->points[i].z = in[i][2];
  }
 
  PointCloud<PointXYZRGB>::Ptr cloudout(new PointCloud<PointXYZRGB>);
  pcl::VoxelGrid<PointXYZRGB> grid;
  grid.setLeafSize (res,res,res);
  grid.setInputCloud (cloud);
  grid.filter (*cloudout);

  // copy back
  out.resize(cloudout->points.size());
  for(int i=0;i<out.size();i++) {
    out[i][0] = cloudout->points[i].x;
    out[i][1] = cloudout->points[i].y;
    out[i][2] = cloudout->points[i].z;
  }
}

Affine3f align_coarse_to_fine(
          const vector<Vector3f>& pts_model, 
          const vector<Vector3f>& pts_data, 
          const vector<float>& res_range)
{
  cout << "Model size: " << pts_model.size() << endl;
  cout << "cloud size: " << pts_data.size() << endl;

  /////////////////////////////
  // coarse align
  float res = res_range[0];
  float padding = 20;

  // convert model to voxels
  Cube<float> dist_xform;
  Affine3f world_to_vol;
  create_voxels(pts_model, res, padding, dist_xform, world_to_vol);

  // perform distance transform on model
  Cube<int> dist_inds(dist_xform.size[0],dist_xform.size[0],dist_xform.size[2]);
  distTransform(dist_xform.data.data(), dist_inds.data.data(), dist_xform.size[2], dist_xform.size[1], dist_xform.size[0]); 
  float maxDist = SQ(dist_xform.size[0]) + SQ(dist_xform.size[1]) + SQ(dist_xform.size[2]);
  maxDist = sqrt(maxDist);
  for(int i=0;i<dist_xform.data.size();i++) dist_xform[i]/=maxDist;
  Vector3f avg_model(0,0,0);
  for(int i=0;i<pts_model.size();i++) avg_model+=pts_model[i];
  avg_model/=pts_model.size();
  vector<Vector3f> pts_data_dec;
  decimate_points(pts_data,pts_data_dec,res);
  cout << "cloud decimate size: " << pts_data_dec.size() << endl;

  // iterate through angles
  float angle_step=30;
  //TODO: allow limit to search
  double best_error = DBL_MAX;
  Affine3f best_P;
  for(float roll=-180; roll<180; roll+=angle_step){
    cout << "Roll: " << roll << endl;
    for(float pitch=-90; pitch<90; pitch+=angle_step){
      for(float yaw=-180; yaw<180; yaw+=angle_step){
        Matrix3f R = ypr2rot(Vector3f(yaw,pitch,roll)*M_PI/180);
        //Matrix3f Rt = R.transpose();
        vector<Vector3f> pts(pts_data_dec.size());
        for(int i=0;i<pts.size();i++) pts[i] = R*pts_data_dec[i]; 
        Vector3f pts_mean(0,0,0);
        for(int i=0;i<pts.size();i++) pts_mean+=pts[i];
        pts_mean/=pts.size();
        Vector3f T=avg_model-pts_mean;
        for(int i=0;i<pts.size();i++) {
          Vector3f p = pts[i]+T;
          Vector4f p4(p[0],p[1],p[2],1);
          p4=world_to_vol*p4; 
          pts[i]=Vector3f(p4[0],p4[1],p4[2]); 
        }

        // compute point pairs and align
        vector<Vector3f> p0,p1;
        point_pairs_from_dist_inds(dist_inds, pts, Matrix3f::Identity(), Vector3f(0,0,0), p0, p1);
        Matrix3f R_opt;
        Vector3f T_opt;
        align_pts_3d(p0,p1,R_opt,T_opt);
        ASSERT_PI(!isnan(R_opt(0,0)));
        ASSERT_PI(!isnan(T_opt[0]));
        
        //cout << dist_xform.size[0] << " " << dist_xform.size[0] << " " << dist_xform.size[0] << endl;
        // compute error
        double error=0;
        for(int i=0;i<pts.size();i++){
          Vector3f pt = R_opt*pts[i]+T_opt;
          pt[0] = min<float>(max<float>(pt[0],0),dist_xform.size[0]-2);
          pt[1] = min<float>(max<float>(pt[1],0),dist_xform.size[1]-2);
          pt[2] = min<float>(max<float>(pt[2],0),dist_xform.size[2]-2);
          //cout << R_opt << endl << pts[i].transpose() << endl << T_opt.transpose() << endl;
          int ul[3] = { (int)floor(pt[0]), (int)floor(pt[1]), (int)floor(pt[2]) };
          double w[3] = {pt[0]-ul[0],pt[1]-ul[1],pt[2]-ul[2] };
          double dist = 
            dist_xform(ul[0]+0,ul[1]+0,ul[2]+0) * (1-w[0])*(1-w[1])*(1-w[2]) +
            dist_xform(ul[0]+0,ul[1]+0,ul[2]+1) * (1-w[0])*(1-w[1])*(  w[2]) +
            dist_xform(ul[0]+0,ul[1]+1,ul[2]+0) * (1-w[0])*(  w[1])*(1-w[2]) +
            dist_xform(ul[0]+0,ul[1]+1,ul[2]+1) * (1-w[0])*(  w[1])*(  w[2]) +
            dist_xform(ul[0]+1,ul[1]+0,ul[2]+0) * (w[0])  *(1-w[1])*(1-w[2]) +
            dist_xform(ul[0]+1,ul[1]+0,ul[2]+1) * (w[0])  *(1-w[1])*(  w[2]) +
            dist_xform(ul[0]+1,ul[1]+1,ul[2]+0) * (w[0])  *(  w[1])*(1-w[2]) +
            dist_xform(ul[0]+1,ul[1]+1,ul[2]+1) * (w[0])  *(  w[1])*(  w[2]);
          error+=SQ(dist);
        }
        error = sqrt(error);
        if(error<best_error){
          cout << error << " " << roll << " " << pitch  << " " << yaw << endl;
          Affine3f opt = Affine3f::Identity();
          opt.linear() = R_opt;
          opt.translation() = T_opt;
          Affine3f orig = Affine3f::Identity();
          orig.linear() = R; 
          orig.translation() = T; 
          best_P = world_to_vol.inverse()*opt*world_to_vol*orig;
          best_error = error;
        }  
      }
    }
  }
  Affine3f pose_init = best_P;
  //return best_P;

  ////////////////////////////////////////////////////////////////////////////////////
  // second pass: iterate through res range
  Affine3f pose = pose_init;
  for(int r=0;r<res_range.size();r++){
      res=res_range[r];
      //if(r!=0) //TODO: dont redo
      create_voxels(pts_model, res, padding, dist_xform, world_to_vol);
      distTransform(dist_xform.data.data(), dist_inds.data.data(), dist_xform.size[2], dist_xform.size[1], dist_xform.size[0]);
      // TODO divide??

      Affine3f P = pose;
      vector<Vector3f> pts(pts_data.size());
      for(int i=0; i<pts.size(); i++){
        pts[i] = P*pts_data[i];
      }
      decimate_points(pts,pts,res);
      for(int i=0; i<pts.size(); i++){
        pts[i] = world_to_vol*pts[i];
      }
      Affine3f result = optimize_pose_with_directions(dist_inds,pts,Affine3f::Identity());
      
      P = result;
      P = world_to_vol.inverse() * P * world_to_vol * pose;
      pose = P;
  }

  Vector3f xyz = pose.translation();
  Vector3f ypr = rot2ypr(pose.linear());

  cout << xyz.transpose() << " " << ypr.transpose()*180/M_PI << endl;
  
  return pose;

}

} //namespace PointCloudFitting