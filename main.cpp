#include "opencv2/opencv.hpp"
#include "opencv2/xfeatures2d.hpp"
#include <fstream>
#include <opencv2/xfeatures2d/nonfree.hpp>

using namespace std;
using namespace cv;
using namespace cv::xfeatures2d;

// Perform a single thinning iteration, which is repeated until the skeletization is finalized
void thinningIteration(Mat& im, int iter)
{
	Mat marker = Mat::zeros(im.size(), CV_8UC1);
	for (int i = 1; i < im.rows - 1; i++)
	{
		for (int j = 1; j < im.cols - 1; j++)
		{
			uchar p2 = im.at<uchar>(i - 1, j);
			uchar p3 = im.at<uchar>(i - 1, j + 1);
			uchar p4 = im.at<uchar>(i, j + 1);
			uchar p5 = im.at<uchar>(i + 1, j + 1);
			uchar p6 = im.at<uchar>(i + 1, j);
			uchar p7 = im.at<uchar>(i + 1, j - 1);
			uchar p8 = im.at<uchar>(i, j - 1);
			uchar p9 = im.at<uchar>(i - 1, j - 1);

			int A = (p2 == 0 && p3 == 1) + (p3 == 0 && p4 == 1) +
				(p4 == 0 && p5 == 1) + (p5 == 0 && p6 == 1) +
				(p6 == 0 && p7 == 1) + (p7 == 0 && p8 == 1) +
				(p8 == 0 && p9 == 1) + (p9 == 0 && p2 == 1);
			int B = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;
			int m1 = iter == 0 ? (p2 * p4 * p6) : (p2 * p4 * p8);
			int m2 = iter == 0 ? (p4 * p6 * p8) : (p2 * p6 * p8);

			if (A == 1 && (B >= 2 && B <= 6) && m1 == 0 && m2 == 0)
				marker.at<uchar>(i, j) = 1;
		}
	}

	im &= ~marker;
}

// Function for thinning any given binary image within the range of 0-255. If not you should first make sure that your image has this range preset and configured!
void thinning(Mat& im)
{
	// Enforce the range tob e in between 0 - 255
	im /= 255;

	Mat prev = Mat::zeros(im.size(), CV_8UC1);
	Mat diff;

	do {
		thinningIteration(im, 0);
		thinningIteration(im, 1);
		absdiff(im, prev, diff);
		im.copyTo(prev);
	} while (countNonZero(diff) > 0);

	im *= 255;
}

int main(int argc, const char** argv)
{
	int trainSize = 100;
	vector<Mat>descriptors(trainSize);
	int i = 0, threshold_harris = 125;
	Mat descriptors2; //for test
	cout << "1 : for train\n2: for test \n";
	int x = 1;
	while (x == 1 || x == 2) {
		cout << "train/test : ";
		cin >> x;

		// Read in an input image - directly in grayscale CV_8UC1
		string s;
		if (x == 2) {

			cout << "img path : ";
			cin >> s;
			Mat input = imread(s, IMREAD_GRAYSCALE);

			// Binarize the image, through local b
			Mat input_binary;
			threshold(input, input_binary, 0, 255, CV_THRESH_BINARY_INV | CV_THRESH_OTSU); //automatic threshold to obtain a good contrast

			// Compare both
			Mat container(input.rows, input.cols * 2, CV_8UC1);
			input.copyTo(container(Rect(0, 0, input.cols, input.rows)));
			input_binary.copyTo(container(Rect(input.cols, 0, input.cols, input.rows)));
			imshow("input versus binary", container); waitKey(0);

			// Now apply the thinning algorithm
			Mat input_thinned = input_binary.clone();
			thinning(input_thinned);

			// Compare both
			input_binary.copyTo(container(Rect(0, 0, input.cols, input.rows)));
			input_thinned.copyTo(container(Rect(input.cols, 0, input.cols, input.rows)));
			imshow("binary versus thinned", container); waitKey(0);

			// Now lets detect the strong minutiae using Haris corner detection
			Mat harris_corners, harris_normalised;
			harris_corners = Mat::zeros(input_thinned.size(), CV_32FC1);
			cornerHarris(input_thinned, harris_corners, 2, 3, 0.04, BORDER_DEFAULT);
			normalize(harris_corners, harris_normalised, 0, 255, NORM_MINMAX, CV_32FC1, Mat());




			// Select the strongest corners that you want
			int threshold_harris = 125;
			vector<KeyPoint> keypoints;

			// Make a color clone for visualisation purposes
			Mat rescaled;
			convertScaleAbs(harris_normalised, rescaled);
			Mat harris_c(rescaled.rows, rescaled.cols, CV_8UC3);
			Mat in[] = { rescaled, rescaled, rescaled };
			int from_to[] = { 0,0, 1,1, 2,2 };
			mixChannels(in, 3, &harris_c, 1, from_to, 3);
			for (int x = 0; x < harris_normalised.cols; x++) {
				for (int y = 0; y < harris_normalised.rows; y++) {
					if ((int)harris_normalised.at<float>(y, x) > threshold_harris) {
						// Draw or store the keypoint location here, just like you decide. In our case we will store the location of the keypoint
						circle(harris_c, Point(x, y), 5, Scalar(0, 255, 0), 1);
						circle(harris_c, Point(x, y), 1, Scalar(0, 0, 255), 1);
						keypoints.push_back(KeyPoint(x, y, 1));
					}
				}
			}
			imshow("corner detection", harris_c); waitKey(0);

			// Compare both
			Mat ccontainer(input.rows, input.cols * 2, CV_8UC3);
			Mat input_thinned_c = input_thinned.clone();
			cvtColor(input_thinned_c, input_thinned_c, CV_GRAY2BGR);
			input_thinned_c.copyTo(ccontainer(Rect(0, 0, input.cols, input.rows)));
			harris_c.copyTo(ccontainer(Rect(input.cols, 0, input.cols, input.rows)));
			imshow("thinned versus selected corners", ccontainer); waitKey(0);

			// Calculate the ORB descriptor based on the keypoint
			Ptr<Feature2D> orb_descriptor = ORB::create();
			orb_descriptor->compute(input_thinned, keypoints, descriptors2);

			// Now lets match test descriptor  with matrix of train descriptors
			// Create the matcher interface
			Ptr<DescriptorMatcher> matcher = DescriptorMatcher::create("BruteForce-Hamming");
			vector< DMatch > matches;
			
			// Loop over matches and multiply
			// Return the matching certainty score
			double score = 0.0,minScore=100000 ;
			for (int j = 0; j <= i; j++) {
				matcher->match(descriptors[j], descriptors2, matches);
	
				for (int it = 0; it < matches.size(); it++) {
					DMatch current_match = matches[it];
					score = score + current_match.distance;
				}
				cout << score << "   ";
				if (score < minScore)
					minScore = score;
				score = 0;
			}
			cout << endl << " min difference distance   = " << score << endl;
			if (minScore == 100000)cout << "it is not found! " << endl;
			waitKey(0);

		}
		else if (x == 1) { //train

			cout << "img path : ";
			cin >> s;

			Mat input2 = imread(s, IMREAD_GRAYSCALE);
			Mat input_binary2;
			threshold(input2, input_binary2, 0, 255, CV_THRESH_BINARY_INV | CV_THRESH_OTSU);
			Mat input_thinned2 = input_binary2.clone();
			thinning(input_thinned2);
			Mat harris_corners2, harris_normalised2;
			harris_corners2 = Mat::zeros(input_thinned2.size(), CV_32FC1);
			cornerHarris(input_thinned2, harris_corners2, 2, 3, 0.04, BORDER_DEFAULT);
			normalize(harris_corners2, harris_normalised2, 0, 255, NORM_MINMAX, CV_32FC1, Mat());
			vector<KeyPoint> keypoints2;
			Mat rescaled2;
			convertScaleAbs(harris_normalised2, rescaled2);
			Mat harris_c2(rescaled2.rows, rescaled2.cols, CV_8UC3);
			Mat in2[] = { rescaled2, rescaled2, rescaled2 };
			int from_to2[] = { 0,0, 1,1, 2,2 };
			mixChannels(in2, 3, &harris_c2, 1, from_to2, 3);
			for (int x = 0; x < harris_normalised2.cols; x++) {
				for (int y = 0; y < harris_normalised2.rows; y++) {
					if ((int)harris_normalised2.at<float>(y, x) > threshold_harris) {
						// Draw or store the keypoint location here, just like you decide. In our case we will store the location of the keypoint
						circle(harris_c2, Point(x, y), 5, Scalar(0, 255, 0), 1);
						circle(harris_c2, Point(x, y), 1, Scalar(0, 0, 255), 1);
						keypoints2.push_back(KeyPoint(x, y, 1));
					}
				}
			}
			//imshow(s, harris_c2); waitKey(0);
			Ptr<Feature2D> orb_descriptor = ORB::create();
			orb_descriptor->compute(input_thinned2, keypoints2, descriptors[i]);

			i++;

		}
	}
	return 0;
}

