#include "findHand.h"
#include <iostream>
#include <errno.h>
#include <time.h>

int main() {
	dllStart("./info.txt", "./queue.txt");
	return 0;
}

int dllStart(const char* infoChar, const char* queueChar) {
	string infoFile(infoChar);
	string queueFile(queueChar);

	logFile.open("./log.txt");
	logFile.close();
	
	writeLog("----dll start----\n");

	inInfo.open(infoFile);
	if (errHandle(getInfo(), "get info")) return 5;
	inInfo.close();

	/* open camera */
	cap.open(0);
	if (errHandle(!cap.isOpened(), "open camera")) return 1;

	/* set variables */
	pastPose = 0;
	pose = CENTER;

	outQueue.open(queueFile, ios_base::trunc);
	if (errHandle(outQueue.fail(), "queue file open")) return 6;

	/* start */
	if (info.first) {	// ó�� �����ѰŶ�� �� ������ �� ����
		info.first = 0;

		outInfo.open(infoFile);
		outInfo.close();

		string str = "first/" + to_string(info.first) + "\n";
		writeOutInfo(infoFile, str);

	/* load classifier to find face */
		if (errHandle(!faceCascade.load("./haarcascade_frontalface_default.xml"),
			"load classifier cascade")) return 4;

		detectFace();
	}

	setImageSize();
	dllUpdate();

	return 0;
}

int dllUpdate() {
	int i = 0;
	while (true) {
		cap.read(img);				// ī�޶�κ��� �̹��� �о��

		if (img.empty()) {			// ������ ����� �ȵ����� ����
			writeLog("fail:    read video err\n");
			return 1;
		}

		string str = to_string(i++) + " : success - read video\n";
		writeLog(str);

		try { //CV::exception ����ó�� handling
			if (waitKey(10) == 27) return 0;		// esc ������ ����

			hideFace();								// �� ������
			fingerDetector(handColor());			// �� ��ġ ���� (���� ù��° ���� ������)
		}

		catch (cv::Exception& e) {
			string err_msg = e.what();
			string str = "exception caught : " + err_msg + "\n";
			writeLog(str);
		}
	}

}

int getInfo() {
	string temp;

	string t;
	int a;

	for (int i = 0; i < sizeof(str)/sizeof(string); i++) {
		getline(inInfo, temp);

		if (temp.find(str[i]) != 0) {
			writeLog("fail:    invalid info form err\n");
			inInfo.close();
			return 1;
		}

		if (i == 0) {
			t = temp.substr(str[i].size() + 1);
			info.first = stoi(t);
		}

		else if (i == 1 || i == 2) {
			t = temp.substr(str[i].size() + 1);
			
			a = t.find('/');
			info.facePoint[i-1].x = stoi(t.substr(0, a));
			info.facePoint[i-1].y = stoi(t.substr(a + 1));
		}
	}

	inInfo.close();
	return 0;
}

int handColor() {
	Scalar handUpper(23, 255, 255);		// �� �� upper boundary
	Scalar handLower(0, 30, 50);		// �� �� lower boundary
	Mat imgHSV;							// RGB -> HSV�� �̹���
	Size blurS(20.0, 20.0);				// �� ������
	Point blurP(-1, -1);				// �� ����Ʈ?
	vector< vector<Point> > contours;	// ������ ����
	int handIdx;						// ������ ���� �� ���� index
	Mat range;							// ����� ���ܵΰ� ���̳ʸ� ó���� �̹���

	cvtColor(img, imgHSV, COLOR_BGR2HSV);			// HSV�� ��ȯ�Ѵ�
	inRange(imgHSV, handLower, handUpper, range);	// boundary ���� ���� ���̳ʸ� �̹����� ��ȯ (����� ���ܵα�)
	blur(range, range, blurS, blurP);				// ��ó��
	threshold(range, range, 180, 255, THRESH_BINARY);	// threshold

	findContours(range, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);	// ������ ã��

	handIdx = findMaxContour(contours);
	// ������ �� ���� ū�� ã�� -> �װ� ��!

	if (handIdx == -1) {	// ũ�Ⱑ �ʹ� ������ ���� �ƴϴ�
		hand.x = -1;
		hand.y = -1;
	}
	else {
		hand.x = contours[handIdx][0].x;
		hand.y = contours[handIdx][0].y;
	}

	drawContours(img, contours, handIdx, Scalar(0, 255, 0), 2, 8);		// �� ����� ������ ����� �׸���

	return handIdx;
}

int findMaxContour(vector< vector<Point> > contours) {	// �ĺ� �� �� ��󳻱�
	int max = 6000;	// ���̶�� ũ�Ⱑ 6000 �̻� �Ǿ����
	int idx = -1;	// 6000�� �Ѵ°� ���� ��� �ƹ��͵� ������ �ν� x
	int area;

	int count = contours.size();

	for (int i = 0; i < count; i++) {
		area = contourArea(contours[i]);
		if (area > max) {
			idx = i;
		}
	}

	return idx;
}

void fingerDetector(int idx) {
	findPose(idx);

	int nowPose = pose;

	if (nowPose == pastPose);
	else {
		outQueue.seekp(0, ios::end);
		outQueue << nowPose;

		pastPose = nowPose;
	}

	return;
}

int findPose(int idx) {
	if (idx == -1) {
		pose = CENTER;
		return 0;
	}

	int midptx = imgW / 2;
	int handX = hand.x;

	if (handX < midptx) pose = LEFT;
	else if (handX > midptx) pose = RIGHT;

	return 0;
}

void setImageSize() {
	cap.read(img);
	imgH = img.rows;
	imgW = img.cols;
	writeLog("success: set image size\n");
}

void hideFace() {
	Mat dst;
	flip(img, img, 1);		//�¿����
	resize(img, dst, Size(imgW * 0.4, imgH * 0.4), 0, 0);
	imshow("display", dst);
	moveWindow("display", 20, 20);

	rectangle(img, info.facePoint[0], info.facePoint[1], Scalar(0, 0, 0), -1);// �� ������
	rectangle(img, Point(0, 0), Point(imgW, info.facePoint[0].y), Scalar(0, 0, 0), -1);	// �� ���� ������
	rectangle(img, Point(0, info.facePoint[1].y * 0.8), Point(imgW, imgH), Scalar(0, 0, 0), -1);	// �� �Ʒ��� ������
}

void detectFace() {
	Mat gray;
	vector<Rect> facePos; //�� ��ġ ����

	do {
		cap.read(img);

		cvtColor(img, gray, COLOR_BGR2GRAY);
		faceCascade.detectMultiScale(gray, facePos, 1.1, 3, 0, Size(30, 30)); //�� ����

	} while (!facePos.size());

	/* facePos ũ�Ⱑ 0�̸�
	   �� ������ �� �� ���̹Ƿ� ���� �ɶ����� ����Ѵ� */

	info.facePoint[0] = Point(facePos[0].x, facePos[0].y * 0.7);	// �̸����� �� ������
	info.facePoint[1] = Point(facePos[0].x + facePos[0].width,
		facePos[0].y + (1.5 * facePos[0].height));	// ����� ������

	string str1, str2;
	str1 = "point1/" + to_string(info.facePoint[0].x) + "/" + to_string(info.facePoint[0].y) + "\n";
	str2 = "point2/" + to_string(info.facePoint[1].x) + "/" + to_string(info.facePoint[1].y) + "\n";
	
	writeOutInfo("./info.txt", str1);
	writeOutInfo("./info.txt", str2);

	writeLog("success: find face\n");
}

void writeLog(string str) {
	logFile.open("./log.txt", ios::app);
	logFile << str;
	logFile.close();
}

void writeOutInfo(string filePath, string str) {
	outInfo.open(filePath, ios::app);
	outInfo << str;
	outInfo.close();
}

int errHandle(int err, string str) {
	string temp;
	if (err) {
		temp += "fail:	";
	}
	else {
		temp += "success: ";
	}

	temp += str + "\n";
	writeLog(temp);
	return err;
}