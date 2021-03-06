#include "ukf.h"
#include "tools.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/** 
 * Initializes Unscented Kalman filter
 * This is scaffolding, do not modify
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 8; //change
  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 0.35;
  
  //DO NOT MODIFY measurement noise values below these are provided by the sensor manufacturer.
  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  //DO NOT MODIFY measurement noise values above these are provided by the sensor manufacturer.
  
  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */
  
  time_us_ = 0.0;

  is_initialized_ = false;

  //* State dimension
  n_x_ = 5;

  //* Augmented state dimension
  n_aug_ = 7;
  
  //* Sigma point spreading parameter
  lambda_ = 3- n_x_;
  //lambda_ = 3-n_aug_;

  //* Weights of sigma points
  weights_ = VectorXd(2*n_aug_ +1);
   
  //*create matrix with predictected sigma points
  Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ +1);
  
  //*NIS for laser
  NIS_laser_ = 0.0;
  
  //*NIS for radar
  NIS_radar_ = 0.0;

}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */

  if(!is_initialized_){
  	//Initialize 
  	//cout<< "UKF Initialization:" << endl;
  	x_ << 1,1,0,0,0; //free set
    
	//create covariance matrix. Data comes from the class 
	P_ <<  0.0043,   -0.0013,    0.0030,   -0.0022,   -0.0020,
	      -0.0013,    0.0077,    0.0011,    0.0071,    0.0060,
	       0.0030,    0.0011,    0.0054,    0.0007,    0.0008,
	      -0.0022,    0.0071,    0.0007,    0.0098,    0.0100,
	      -0.0020,    0.0060,    0.0008,    0.0100,    0.0123;
  
	if (meas_package.sensor_type_ == MeasurementPackage::RADAR && use_radar_) {
      /**
      Convert radar from polar to cartesian coordinates and initialize state.
      */
		double ro = meas_package.raw_measurements_(0);
		double phi = meas_package.raw_measurements_(1);
		//float ro_dot = meas_package.raw_measurements_(2); CTRV model means costant velocity.
		x_(0) = ro * cos(phi);
		x_(1) = ro * sin(phi);      
    }
    else if (meas_package.sensor_type_ == MeasurementPackage::LASER  && use_laser_) {
      /**
      Initialize state.
      */
		x_(0) = meas_package.raw_measurements_(0);
		x_(1) = meas_package.raw_measurements_(1);
    }
    
    // initialize first
    time_us_ = meas_package.timestamp_;
    is_initialized_ = true;
    return ;  
  }
  
	/****** predict ******/
	// Calculate new elasped time and Time is measured in seconds
	double delta_t = (meas_package.timestamp_ - time_us_) / 1000000.0;    
	time_us_ = meas_package.timestamp_;
	
	Prediction(delta_t);

	/****** update ******/	
	if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
		UpdateRadar(meas_package);
	} else if(meas_package.sensor_type_ == MeasurementPackage::LASER){
		UpdateLidar(meas_package);
	}  
}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */
	
	/****** 1. Generate sigma points ******/  

    //create sigma point matrix
	MatrixXd Xsig = MatrixXd(n_x_, 2 * n_x_ + 1);

	//calculate square root of P
	MatrixXd A = P_.llt().matrixL();

	//set first column of sigma point matrix
	Xsig.col(0)  = x_;

	//lamda_
	lambda_ = 3 - n_x_;

	//set remaining sigma points
	for (int i = 0; i < n_x_; i++){
		Xsig.col(i+1)     = x_ + sqrt(lambda_+n_x_) * A.col(i);
		Xsig.col(i+1+n_x_) = x_ - sqrt(lambda_+n_x_) * A.col(i);
	}

	/*****1.1 augmented sigma points******/
	//define spreading parameter
  	lambda_ = 3 - n_aug_;
	  
	//create augmented mean vector
	VectorXd x_aug_ = VectorXd(n_aug_);
	
	//create augmented state covariance
	MatrixXd P_aug_ = MatrixXd(n_aug_, n_aug_);
	
	//create sigma point matrix
	MatrixXd Xsig_aug_ = MatrixXd(n_aug_, 2 * n_aug_ + 1);  
    
	//create augmented mean state
	x_aug_.head(5) = x_;
	x_aug_(5) = 0;
	x_aug_(6) = 0;
	
	//create augmented covariance matrix
	P_aug_.fill(0.0);
	P_aug_.topLeftCorner(5,5) = P_;
	P_aug_(5,5) = std_a_ * std_a_;
	P_aug_(6,6) = std_yawdd_ * std_yawdd_;
	
	//create square root matrix
	MatrixXd L = P_aug_.llt().matrixL();
	
	//create augmented sigma points
	Xsig_aug_.col(0)  = x_aug_;
	for (int i = 0; i< n_aug_; i++){
		Xsig_aug_.col(i+1)       = x_aug_ + sqrt(lambda_+n_aug_) * L.col(i);
		Xsig_aug_.col(i+1+n_aug_) = x_aug_ - sqrt(lambda_+n_aug_) * L.col(i);
	}

	/****** 2.predict sigma point ******/

	//create matrix with predicted sigma points as columns
	//MatrixXd Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ + 1);
	//predict sigma points
	for (int i = 0; i< 2*n_aug_+1; i++) {

		//extract values for better readability
		double p_x = Xsig_aug_(0,i);
		double p_y = Xsig_aug_(1,i);
		double v = Xsig_aug_(2,i);
		double yaw = Xsig_aug_(3,i);
		double yawd = Xsig_aug_(4,i);
		double nu_a = Xsig_aug_(5,i);
		double nu_yawdd = Xsig_aug_(6,i);
		
		//predicted state values
		double px_p, py_p;
		
		//avoid division by zero
		if (fabs(yawd) > 0.001) {
			px_p = p_x + v/yawd * ( sin (yaw + yawd*delta_t) - sin(yaw));
			py_p = p_y + v/yawd * ( cos(yaw) - cos(yaw+yawd*delta_t) );
		} else {
			px_p = p_x + v*delta_t*cos(yaw);
			py_p = p_y + v*delta_t*sin(yaw);
		}
		
		double v_p = v;
		double yaw_p = yaw + yawd*delta_t;
		double yawd_p = yawd;
		
		//add noise
		px_p = px_p + 0.5*nu_a*delta_t*delta_t * cos(yaw);
		py_p = py_p + 0.5*nu_a*delta_t*delta_t * sin(yaw);
		v_p = v_p + nu_a*delta_t;
		
		yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
		yawd_p = yawd_p + nu_yawdd*delta_t;
		
		//write predicted sigma point into right column
		Xsig_pred_(0,i) = px_p;
		Xsig_pred_(1,i) = py_p;
		Xsig_pred_(2,i) = v_p;
		Xsig_pred_(3,i) = yaw_p;
		Xsig_pred_(4,i) = yawd_p;
	}
	
	
	/****** 3.predict Mean and Covariance******/
	
	// set weights_
	double weights_0 = lambda_/(lambda_+n_aug_);
	weights_(0) = weights_0;
	for (int i=1; i<2*n_aug_+1; i++) {  //2n+1 weights
		double weight = 0.5/(n_aug_+lambda_);
		weights_(i) = weight;
	}
	
	//predicted state mean
	x_.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //iterate over sigma points
		x_ = x_+ weights_(i) * Xsig_pred_.col(i);
	}
	
	//predicted state covariance matrix
	P_.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //iterate over sigma points
	
		// state difference
		VectorXd x_diff = Xsig_pred_.col(i) - x_;
		//angle normalization
		while (x_diff(3)> M_PI) x_diff(3) -= 2.*M_PI;
		while (x_diff(3)<-M_PI) x_diff(3) += 2.*M_PI;
		
		P_ = P_ + weights_(i) * x_diff * x_diff.transpose();
	}		
}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */

    /****** 1. predict Radar Measurement******/
    
    //set measurement dimension, lidar can measure px, py
	int n_z = 2; 
				
	//create matrix for sigma points in measurement space
  	MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);
  	
	//transform sigma points into measurement space
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
	
		// extract values for better readibility
		double p_x = Xsig_pred_(0,i);
		double p_y = Xsig_pred_(1,i);
		
		// measurement model
		Zsig(0,i) = p_x;                      //r
		Zsig(1,i) = p_y;                      //phi
	}	

	//mean predicted measurement
	VectorXd z_pred = VectorXd(n_z);
	z_pred.fill(0.0);
	for (int i=0; i < 2*n_aug_+1; i++) {
		z_pred = z_pred + weights_(i) * Zsig.col(i);
	}
	
	//innovation covariance matrix S
	MatrixXd S = MatrixXd(n_z,n_z);
	S.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
		//residual
		VectorXd z_diff = Zsig.col(i) - z_pred;
		
		S = S + weights_(i) * z_diff * z_diff.transpose();
	}
	
	//add measurement noise covariance matrix
	MatrixXd R = MatrixXd(n_z,n_z);
	R <<    std_laspx_*std_laspx_, 0, 
	        0,std_laspy_*std_laspy_; 
	    
	S = S + R;
	
	/****** 2.Update ******/
	 
	//create matrix for cross correlation Tc
	MatrixXd Tc = MatrixXd(n_x_, n_z);	
			 
	//calculate cross correlation matrix
	Tc.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
		//residual
		VectorXd z_diff = Zsig.col(i) - z_pred;
		// state difference
		VectorXd x_diff = Xsig_pred_.col(i) - x_;
	
		Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
	}
	
	//Kalman gain K;
	MatrixXd K = Tc * S.inverse();
	
	//extract measurement 
	VectorXd z = meas_package.raw_measurements_;
	
	//residual
	VectorXd z_diff = z - z_pred;

	//calculate NIS 
	NIS_radar_ = z_diff.transpose()*S.inverse()* z_diff;

	//update state mean and covariance matrix
	x_ = x_ + K * z_diff;
	P_ = P_ - K*S*K.transpose();	
		
}
/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */

void UKF::UpdateRadar(MeasurementPackage meas_package){
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */
    /****** 1. predict Radar Measurement******/
	
	//set measurement dimension, radar can measure r, phi, and r_dot
	int n_z = 3; 
				
	//create matrix for sigma points in measurement space
  	MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);
  	
	//transform sigma points into measurement space
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
		
		// extract values for better readibility
		double p_x = Xsig_pred_(0,i);
		double p_y = Xsig_pred_(1,i);
		double v  = Xsig_pred_(2,i);
		double yaw = Xsig_pred_(3,i);
		
		double v1 = cos(yaw)*v;
		double v2 = sin(yaw)*v;
		
		// measurement model
		Zsig(0,i) = sqrt(p_x*p_x + p_y*p_y);                        //r
		Zsig(1,i) = atan2(p_y,p_x);                                 //phi
		Zsig(2,i) = (p_x*v1 + p_y*v2 ) / sqrt(p_x*p_x + p_y*p_y);   //r_dot
	}
	
	// mean predicted measurement
	VectorXd z_pred = VectorXd(n_z);
	z_pred.fill(0.0);
	for (int i=0; i < 2*n_aug_+1; i++){
		z_pred = z_pred + weights_(i) * Zsig.col(i);
	}
	
	// innovation covariance matrix S
	MatrixXd S = MatrixXd(n_z,n_z);
	S.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
		//residual
		VectorXd z_diff = Zsig.col(i) - z_pred;
		// angle normalization
		while (z_diff(1) > M_PI) z_diff(1) -= 2.*M_PI;
		while (z_diff(1) < -M_PI) z_diff(1) += 2.*M_PI;
		S = S + weights_(i) * z_diff * z_diff.transpose();
	}
	
	// add measurement noise covariance matrix
	MatrixXd R = MatrixXd(n_z,n_z);
	R <<  std_radr_*std_radr_, 0, 0,
	      0, std_radphi_*std_radphi_, 0,
	      0, 0, std_radrd_*std_radrd_;
	S = S + R;
	
	//****** 2.Update ******//
	//incoming radar measurement data, extract measurement as VectorXd;
	VectorXd z = meas_package.raw_measurements_;
	 
	//create matrix for cross correlation Tc
	MatrixXd Tc = MatrixXd(n_x_, n_z);	
			 
	//calculate cross correlation matrix
	Tc.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
	
		//residual
		VectorXd z_diff = Zsig.col(i) - z_pred;
		//angle normalization
		while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
		while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;
		
		// state difference
		VectorXd x_diff = Xsig_pred_.col(i) - x_;
		//angle normalization
		while (x_diff(3) > M_PI) x_diff(3) -= 2.*M_PI;
		while (x_diff(3) < -M_PI) x_diff(3) += 2.*M_PI;
		
		Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
	}
	
	//Kalman gain K;
	MatrixXd K = Tc * S.inverse();
	
	//residual
	VectorXd z_diff = z - z_pred;
	
	//angle normalization
	while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
	while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

	//calculate NIS 
	NIS_radar_ = z_diff.transpose()*S.inverse()* z_diff;

	//update state mean and covariance matrix
	x_ = x_ + K * z_diff;
	P_ = P_ - K*S*K.transpose();	
 	//ok!
}
