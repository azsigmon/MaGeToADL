#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <fstream>

#include "ADLCluster.hh"

ADLCluster::ADLCluster()
{

}

ADLCluster::~ADLCluster()
{

}

typedef struct { double x, y; int group; } point_t, *point;

double randf(double m)
{
    return m * rand() / (RAND_MAX - 1.);
}

std::vector<int> SortHitsId(int Nhits,int detId,int*hIddet)
{
  std::vector<int> Id;

  for (int i=0;i<Nhits;i++) if(hIddet[i] == detId) Id.push_back(i);

  return Id;
}

point set_rz(int Npts, float* hr,float*hz,std::vector<int> &HitsId)
{
  point p, pt = (point) malloc(sizeof(point_t) * Npts);
  int i =0;
  /* note: this is not a uniform 2-d distribution */
  for (p = pt + Npts; p-- > pt;) {
    p->x = hr[HitsId[i]];
    p->y = hz[HitsId[i]];
    //    printf("    Set data : %d %.03f %.03f \n",HitsId[i],hr[HitsId[i]],hz[HitsId[i]]);
    i++;
  }
  
  return pt;
}

double GetDist(double x0,double y0,double x1,double y1){
    double x = x0-x1;
    double y = y0-y1;
    return x*x+y*y;
}

double mean(double x,double y){
    return (x+y)/2.;
}

double dist2(point a, point b)
{
    double x = a->x - b->x, y = a->y - b->y;
    return x*x + y*y;
}

int nearest(point pt, point cent, int n_cluster, double *d2)
{
    int i, min_i;
    point c;
    double d, min_d;
    
#	define for_n for (c = cent, i = 0; i < n_cluster; i++, c++)
    for_n {
        min_d = HUGE_VAL;
        min_i = pt->group;
        for_n {
            if (min_d > (d = dist2(c, pt))) {
                min_d = d; min_i = i;
            }
        }
    }
    if (d2) *d2 = min_d;
    return min_i;
}

void kpp(point pts, int len, point cent, int n_cent)
{
#	define for_len for (j = 0, p = pts; j < len; j++, p++)
    int i, j;
    int n_cluster;
    double sum, *d = (double*)malloc(sizeof(double) * len);
    
    point p, c;
    cent[0] = pts[ rand() % len ];
    for (n_cluster = 1; n_cluster < n_cent; n_cluster++) {
        sum = 0;
        for_len {
            nearest(p, cent, n_cluster, d + j);
            sum += d[j];
        }
        sum = randf(sum);
        for_len {
            if ((sum -= d[j]) > 0) continue;
            cent[n_cluster] = pts[j];
            break;
        }
    }
    for_len p->group = nearest(p, cent, n_cluster, 0);
    free(d);
}

std::vector<std::vector<double> > lloyd(point pts, int len, int n_cluster,int detId)
{
    int i, j, min_i;
    int changed;
    
    point cent = (point)malloc(sizeof(point_t) * n_cluster), p, c;
    
    /* assign init grouping randomly */
    //for_len p->group = j % n_cluster;
    
    /* or call k++ init */
    kpp(pts, len, cent, n_cluster);
    
    do {
        /* group element for centroids are used as counters */
        for_n { c->group = 0; c->x = c->y = 0; }
        for_len {
            c = cent + p->group;
            c->group++;
            c->x += p->x; c->y += p->y;
        }
        for_n { c->x /= c->group; c->y /= c->group; }
        
        changed = 0;
        /* find closest centroid of each point */
        for_len {
            min_i = nearest(p, cent, n_cluster, 0);
            if (min_i != p->group) {
                changed++;
                p->group = min_i;
            }
        }
    } while (changed > (len >> 10)); /* stop when 99.9% of points are good */
    
    for_n { c->group = i; }
    
    std::vector<std::vector<double> > clusterPos;
    std::vector<double> clusterPosX,clusterPosY,clusterDetID;
        
    for(i = 0; i < n_cluster; i++, cent++) {
        clusterPosX.push_back(cent->x);
        clusterPosY.push_back(cent->y);
        clusterDetID.push_back(detId);
    }
    clusterPos.push_back(clusterPosX);
    clusterPos.push_back(clusterPosY);
    clusterPos.push_back(clusterDetID);

    return clusterPos;
}

//int CheckClusters(int Nhits,int Npts,float*hr,float* hz,int* hIddet,int detID,std::vector<std::vector<double> > &cluster, double RMS){
int CheckClusters(int Npts,float*hr,float* hz,std::vector<int> &HitsId,int detID,std::vector<std::vector<double> > &cluster, double RMS){

    int Ncls = cluster[0].size();
    
    int Nin = 0, Nin2 = 0, Nout = 0;
    double dist;
    int IDin[Npts];
    
    //printf("   Enter check clusters : %d / %d %d \n", Npts,HitsId.size(),Ncls);

    for(int j = 0;j<HitsId.size();j++) IDin[j] = 0;
    
    for(int i = 0;i<Ncls;i++){
        Nout = 0;
	//printf("   cluster %i check : %.04f %.04f \n",i,cluster[0][i],cluster[1][i]); 
        for(int j = 0;j<HitsId.size();j++){
	  //printf("   hit %d check pos : %.04f %.04f \n",j,hr[j],hz[j]); 
	  if(IDin[HitsId[j]] == 0){
	    //printf("   hit %i check : %.04f %.04f %.04f %.04f \n",j,hr[j],hz[j],cluster[0][i],cluster[1][i]); 
	      dist = sqrt(GetDist(hr[HitsId[j]],hz[HitsId[j]],cluster[0][i],cluster[1][i]));
                if(dist <= RMS){ Nin++; IDin[HitsId[j]] = 1;}
		//                else if(dist <= 1.5*RMS){ Nin2++; IDin[j] = 1;}
                else{ 
		  //printf("   hit %i out : %.04f %.04f %.04f %.04f %.04f %.04f \n",j,hr[j],hz[j],cluster[0][i],cluster[1][i],dist,dist-RMS); 
		  Nout++; 
		}
            }
        }
    }
                    
    //printf("Counting : %d %d %.03f %.03f %.03f sum : %.02f \n",Ncls,Npts,double(Nin)/double(Npts),double(Nin2)/double(Npts),double(Nout)/double(Npts),double(Nin+Nin2+Nout)/double(Npts));

    //    if(double(Nin+Nin2)/double(Npts) > 0.99 && double(Nin)/double(Npts) < 0.99) return 2;
    //    else if(double(Nin)/double(Npts) < 0.99) return 0;
    if(double(Nin)/double(Npts) < 0.99) return 0;
   
    int IDoverlap[Ncls][Ncls];
    int overlap = 0;

    for(int i = 0;i<Ncls;i++)
        for(int j = i;j<Ncls;j++)
            IDoverlap[i][j] = 0;
    
    for(int i = 0;i<Ncls;i++){
        for(int j = i;j<Ncls;j++){
            if(j > i){
                dist = sqrt(GetDist(cluster[0][j],cluster[1][j],cluster[0][i],cluster[1][i]));
                if(dist < 2.*RMS){
                    IDoverlap[i][j] = 1;
                    overlap++;
                }
            }
        }
    }

    if(overlap>0){
      std::vector<double> newclusterX, newclusterY, newclusterID;
      double meanX, meanY;
    
    for(int i = 0;i<Ncls;i++){
        meanX = 0.;
        meanY = 0.;
        overlap = 0;
        
        for(int j = i;j<Ncls;j++){
            if(IDoverlap[i][j] == 1 && cluster[0][j] != 666){
                meanX += mean(cluster[0][i],cluster[0][j]);
                meanY += mean(cluster[1][i],cluster[1][j]);
                cluster[0][j] = 666;
                cluster[1][j] = 666;
                overlap++;
            }
        }
        if(overlap == 0 && cluster[0][i] != 666){
            newclusterX.push_back(cluster[0][i]);
            newclusterY.push_back(cluster[1][i]);
	    newclusterID.push_back(detID);
        }
        else if(cluster[0][i] != 666){
            newclusterX.push_back(meanX/double(overlap));
            newclusterY.push_back(meanY/double(overlap));
 	    newclusterID.push_back(detID);
       }
    }
    
    cluster.clear();
    cluster.push_back(newclusterX);
    cluster.push_back(newclusterY);
    cluster.push_back(newclusterID);
    }
    
    //printf("%lu clusters has been removed \n",Ncls-cluster[0].size());
    
    return 1;
}

std::vector<double> SetClusterEnergy(int Npts,float*hr,float* hz,std::vector<int> detIds,int* hIddet,std::vector<std::vector<double> > &cluster,float*hEdep, double threshold)
{
  int Ncls = cluster[0].size();
  int Ndet = detIds.size();

  std::vector<double> clusterE;

  for(int k = 0;k<Ndet;k++){
    for(int i = 0;i<Ncls;i++){
      if(cluster[2][i] == detIds[k]){

	clusterE.push_back(0.);	  
	for(int j = 0;j<Npts;j++)
	  if(hIddet[j] == detIds[k] && sqrt(GetDist(hr[j],hz[j],cluster[0][i],cluster[1][i])) < threshold) clusterE[i] += hEdep[j];
      }
    }    
  }
  return clusterE;
}

double CheckEdep(int Nhits, std::vector<std::vector<double> > &cluster, float* hEdep)
{
  int Ncls = cluster[0].size();
  double clusterEdep = 0;
  double hitsEdep = 0;

  for(int i = 0;i<Ncls;i++) clusterEdep += cluster[3][i];
  for(int i = 0;i<Nhits;i++) hitsEdep += hEdep[i];

  //  printf("Energy deposited in cluster %lf and hits %lf. Ratio : %lf \n",clusterEdep,hitsEdep,clusterEdep/hitsEdep);

  return clusterEdep/hitsEdep;
}

void ADLCluster::GetRadCoord(int Npts,float* hx,float* hy)
{
  for(int i = 0; i<Npts;i++) hr[i] = sqrt(pow(hx[i],2)+pow(hy[i],2));
}

std::vector<int> GetDetIds(int Npts,int* hIddet){

  int flag = 0;
  std::vector<int> detIds;
  
  detIds.push_back(hIddet[0]);

  for(int i = 1;i<Npts;i++){
    for(int j = 0;j<detIds.size();j++){
      if(hIddet[i] != detIds[j]) flag = 1;
      else {flag = 0; break;}
    }
    if(flag) detIds.push_back(hIddet[i]);
  }

  return detIds;
}

void CheckData(int Npts,point data)
{
  int i;
  point p;
  for(i = 0, p = data; i < Npts; i++, p++) printf("    Check data : %.04f %.04f \n",p->x,p->y);


}

double ADLCluster::LaunchClustering(int Nhits, float* hx,float* hy,float*hz,float* hEdep,int* hIddet)
{
  int debug = 1;
  int K = 1;                // Initial number of clusters 
  int Nstep = 10;           // Maximum number of cluster authorized
  int check = 0;            // Flag to determine the appropriate number of clusters
  int Npts;                 // Number of hits for each detector
  double threshold = 0.1;   // Cluster size in cm

  GetRadCoord(Nhits,hx,hy); // Transform (X,Y) coord to radial coord.
  std::vector<int> detIds = GetDetIds(Nhits,hIddet);

  //if(debug) printf("   Detectors ID : %d \n",detIds.size());
  //if(debug) printf("   Loop over detector ID ");

  for(int j = 0;j<detIds.size();j++){
    check = 0;
    K = 1;
    //if(debug) printf("%d ",j);

    std::vector<std::vector<double> > clustersPosTmp;
    std::vector<int> HitsId = SortHitsId(Nhits,detIds[j],hIddet);
    Npts = HitsId.size();
    point data = set_rz(Npts,hr,hz,HitsId);
    //CheckData(Npts,data);

    for(int i = 0;i<Nstep;i++){
      //printf(" %d %d %d %d / %d ",detIds[j],K,check, Npts,Nhits);
      clustersPosTmp = lloyd(data, Npts, K,detIds[j]);
      //if(debug) printf(" lloyd step : %d ",i);
      check = CheckClusters(Npts,hr,hz,HitsId,detIds[j],clustersPosTmp,threshold);
      //if(debug) printf(" check step : %d ",check);
      if(check == 1) break;
      else K++;
    }
    //if(debug) printf(" Number of cluster : %d ",clustersPosTmp[0].size());
    if(j==0){
      clustersPos.push_back(clustersPosTmp[0]);
      clustersPos.push_back(clustersPosTmp[1]);
      clustersPos.push_back(clustersPosTmp[2]);
    }
    else{
      clustersPos[0].insert(std::end(clustersPos[0]),std::begin(clustersPosTmp[0]),std::end(clustersPosTmp[0]));
      clustersPos[1].insert(std::end(clustersPos[1]),std::begin(clustersPosTmp[1]),std::end(clustersPosTmp[1]));
      clustersPos[2].insert(std::end(clustersPos[2]),std::begin(clustersPosTmp[2]),std::end(clustersPosTmp[2]));
    }

    //if(debug) printf(" set ");
    clustersPosTmp.clear();
    free(data);
  }    

  //if(debug) printf("\n   Set cluster Edep \n");
  clustersPos.push_back(SetClusterEnergy(Nhits,hr,hz,detIds,hIddet,clustersPos,hEdep,threshold));
  //if(debug) printf("   Check Edep \n");
  return CheckEdep(Nhits,clustersPos,hEdep);

}

std::vector<std::vector<double> > ADLCluster::GetClusters(){return clustersPos;}
