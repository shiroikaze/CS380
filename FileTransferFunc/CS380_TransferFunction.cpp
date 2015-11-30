
#include "stdafx.h"
#include <iostream>
#include <iomanip>
//#include <fstream>
//#include <sstream>
#include <boost/asio.hpp>
#include <boost/array.hpp>
//#include <iterator>								// For reading directory
//#include <vector>								// For reading directory
#include <regex>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
//#include <boost/bind.hpp>

using boost::asio::ip::tcp;
const std::string port = "1234";					// Default port
const std::string uploadPath = "./data/";			// Location of file to upload
const std::string downloadPath = "./downloads/";	// Location to put downloaded file
const int chunk_size = 9999;						// Chunk size used. 9999Bytes

//// hash
boost::array<char, 20> checkSumHash(boost::array<char, chunk_size> buf) {

	char hash[20];

	for (int i = 0; i < chunk_size; i++) {
		char c = buf.at(i);

		hash[i % 20] = c ^ hash[i % 20];
	}

	boost::array<char, 20> s;

	for (int i = 0; i < 20; i++) {
		s.at(i) = hash[i];
	}

	return s;
}

//Prints out the hash as hex.
void printOutHash(boost::array<char, 20> hash) {

	std::cout << "Hash: ";
	for (int i = 0; i < 20; i++) {
		unsigned char c = hash.at(i);
		if ((int)c <= 15)
			std::cout << '0' << std::dec;
		std::cout << std::hex << (int)c << std::dec;
	}
	std::cout << std::dec << " ";
}

//Prints out the hash as hex.
std::string printOutHash2(boost::array<char, 20> hash) {
	std::stringstream ss;

	for (int i = 0; i < 20; i++) {
		unsigned char c = hash.at(i);
		if ((int)c <= 15)
			ss << std::hex << '0' << std::dec;
		ss << std::hex << (int)c << std::dec;
	}

	std::string result(ss.str());
	return result;
}

/*
This is for reading inside the directory to find what available files can be used.
Leave this commented out in case we need it in the future

std::string workingDir() {
std::string message;
boost::system::error_code ignored_error;
boost::filesystem::path path(uploadPath);
try {
if (boost::filesystem::exists(path)) {
if (boost::filesystem::is_regular_file(path)) {
std::cout << path << " , size: " << boost::filesystem::file_size(path) << std::endl;
}
else if (boost::filesystem::is_directory(path)) {
//std::cout << "  Contents of " << path << std::endl;
message = "  Contents of " + path.string() + "\n";

std::vector<std::string> v;
for (auto&& x : boost::filesystem::directory_iterator(path))
{
v.push_back(x.path().filename().string());
}
std::sort(v.begin(), v.end());
for (auto&& x : v) {
//std::cout << "    " << x << '\n';
message += "    " + x + "\n";
}
}
else {
std::cout << path << " exists, but is neither a regular file nor a directory\n";
}
}
else {
std::cout << path << " does not exist\n";
}
}
catch (boost::filesystem::filesystem_error& ex) {
std::cout << ex.what() << std::endl;
}
return message;
}
*/

// Server sends file... Correction: Server gets file.
void startServer() {
	std::cout << "SERVER RUNNING... listening on port " << port << ".\n";

	boost::asio::io_service io_service;
	//tcp::resolver resolver(io_service);
	tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), stoi(port)));
	boost::system::error_code error;
	tcp::socket socket(io_service);
	acceptor.accept(socket);

	//This is using the above function. Not needed.
	//boost::asio::write(socket, boost::asio::buffer(workingDir()));

	std::cout << "\nA client has connected.\n";


	boost::asio::streambuf request_buf;
	boost::array<char, chunk_size> buf;

	std::string fileName;
	std::string path;
	size_t fileSize = 0;
	while (true) {
		boost::asio::read_until(socket, request_buf, "\n");							// Read until newline char
		std::istream request_fileName(&request_buf);								// Read in requested filename
		request_fileName >> fileName;
		request_fileName.read(buf.c_array(), 1);									// Eat "\n"
	
	// Get file
	boost::asio::read_until(socket, request_buf, "\n\n");
	std::istream in_stream(&request_buf);
	
	in_stream >> path;
	in_stream >> fileSize;
	in_stream.read(buf.c_array(), 2);							// Eat "\n\n"


	std::cout << "\nfile: " << fileName << " size: " << fileSize << " bytes\n" << std::endl;
	size_t pos = path.find_last_of("/");
	if (pos != std::string::npos)
		path = downloadPath + path.substr(pos + 1);
	std::ofstream outputFile(path.c_str(), std::ios_base::binary);
		if (!outputFile)
		{
			std::cout << "Failed to open " << path << std::endl;
		}
		else {	
			// Purpose is to write extra bytes, possibly omit?
			do {
				in_stream.read(buf.c_array(), (std::streamsize)buf.size());
				std::cout << __FUNCTION__ << " write " << in_stream.gcount() << " bytes.\n";
				outputFile.write(buf.c_array(), in_stream.gcount());
			} while (in_stream.gcount()>0);
			break;
		}
	}

	std::cout << std::endl;
	//Chunk info.
	int currSize = 0;
	int count = 0;

	// Writing the file

	size_t tempLen;
	boost::array<char, chunk_size> tempChunk;

	std::ofstream outputFile(path.c_str(), std::ios_base::binary);
	while (true) {

		size_t len = socket.read_some(boost::asio::buffer(buf), error);
		count++;

		if (count % 2 == 1) {
			tempChunk = buf;
			tempLen = len;
		}
		else {
			boost::array<char, 20> checksum = checkSumHash(tempChunk);

			//buf.c_array();
			//
			//Print out Chunk info.
			currSize += tempLen;
			std::cout << "Receiving Chunk #" << std::setw(5) << std::left << count << ": ";
			std::cout << "Length: " << std::setw(10) << std::left << tempLen << " ";
			std::cout << "Cur Size: " << std::setw(10) << std::left << currSize << " ";


			std::stringstream ss;

			for (int i = 0; i < 20; i++) {
				unsigned char c = buf.at(i);
				if ((int)c <= 15)
					ss << std::hex << '0' << std::dec;
				ss << std::hex << (int)c << std::dec;
			}

			std::string sum1(ss.str());
			std::string sum2 = printOutHash2(checksum);

			//if you comment these next 2 lines, the hashes will match up perfectly, or at least it should.
			//std::cout << "comp: " << sum2 << " "; //this one
			//std::cout << sum1 << std::endl;       //and this one. 

			std::cout << "comp: " << (sum1 == sum2);
			std::cout << std::endl;

			if (tempLen > 0) {
				outputFile.write(tempChunk.c_array(), (std::streamsize)tempLen);
			}
			if (outputFile.tellp() >= (std::fstream::pos_type)(std::streamsize)fileSize)
				break; // file was received
			if (error)
			{
				std::cout << error << std::endl;
				break;
			}
		}
	}
	std::cout << "Received " << outputFile.tellp() << " bytes.\n" << std::endl;

}

/*
void login() {

if () {
startClient();
}
else {
std::cout << "Login failed, exiting program...";
exit(1); }
}
*/

void bigCodeBlock1(std::ostream &request_stream, boost::asio::streambuf &request_buf, tcp::socket &socket, std::ifstream &source, boost::system::error_code error, std::string fileName, int i);
void bigCodeBlock2(std::ostream &request_stream, boost::asio::streambuf &request_buf, tcp::socket &socket, std::ifstream &source, boost::system::error_code error, std::string fileName, int i);
std::vector<std::string> clientLogIn();

/*void loginStart(std::string serverIP) {
	//Log in!
	clientLogIn();
	startClient(serverIP);
}*/

// Client receives file... Correction: Client receives file.
void startClient(std::string serverIP) {
	boost::asio::io_service io_service;
	tcp::resolver resolver(io_service);
	tcp::resolver::query query(serverIP, port);
	tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
	tcp::resolver::iterator end;
	tcp::socket socket(io_service);
	boost::system::error_code error = boost::asio::error::host_not_found;
	// Magical
	while (error && endpoint_iterator != end)
	{
		socket.close();
		socket.connect(*endpoint_iterator++, error);
	}

	if (serverIP == "") { serverIP = "127.0.0.1"; }					//So the below print won't have a blank
	std::cout << "Connected to " << serverIP << ":" << port << std::endl;
	boost::array<char, chunk_size> buf;
	boost::asio::streambuf request_buf;

	/*
	This waits for the server to send a message to continue...
	Used because the server sends back the folder contents, but I removed it so we don't need it anymore.

	size_t len = socket.read_some(boost::asio::buffer(buf), error);
	std::cout.write(buf.data(), len);
	*/


	//Selecting a file to upload
	std::string filePath;
	std::string fileName;
	for (int i = 1; i <= 3; i++) {			//Successful transfer on 1st try, attempt wrong filename & subsequent tries will fail.
		std::cout << "\nSelect a file: ";
		std::getline(std::cin, fileName);
		std::ostream out_stream(&request_buf);											// Send filename for request
		out_stream << fileName << "\n";
		boost::asio::write(socket, request_buf);

		filePath = uploadPath + fileName;												// Concatenate path and filename

		std::ifstream source(filePath, std::ios_base::binary);							// Read in file
		if (!source) {
			std::cout << "Attempt #" << i << ". Failed to open file...\n" << filePath << std::endl;
			if (i == 3)
				std::cout << "Retries exhausted, closing connection...\n" << filePath << std::endl;
		}
		else {
			i = 0;
			size_t fileSize = boost::filesystem::file_size(filePath);					// Get file size
			//boost::asio::streambuf request;
			std::ostream request_stream(&request_buf);
			request_stream << filePath << "\n" << fileSize << "\n\n";					// Send filename and size
			boost::asio::write(socket, request_buf);

			//Sending file
			std::cout << "Sending file...\n";
			//Chunk info counting.
			//int currSize = 0;
			//int count = 0;
			//Choose success/some failure/total failure states here?
			std::string input = "";
			while (true){
				std::cout << "(1) No ASCII-armoring" << std::endl;
				std::cout << "(2) ASCII-armoring" << std::endl;
				std::cout << "Please select: ";
				std::getline(std::cin, input);
				std::cout << std::endl;
				if (input == "1") {
					bigCodeBlock1(request_stream, request_buf, socket, source, error, fileName, i);
					i = 4;			//exits out of for-loop
					break; 
				}
				else if (input == "2") { 
					//bigCodeBlock2(request_stream, request_buf, socket, source, error, fileName, i);
					i = 4;			//exits out of for-loop
					break;
				}
				else {
					std::cout << "Invalid input.\n" << std::endl;
				}
			}
			/*while (true){
				
				std::cout << "(1) Successful transfer"<< std::endl;
				std::cout << "(2) Partial failure" << std::endl;
				std::cout << "(3) Total failure" << std::endl;
				std::cout << "Please select: ";
				std::getline(std::cin, input);
				std::cout << std::endl;
				if (input == "1") {
					while (true) {
						if (source.eof() == false) {
							source.read(buf.c_array(), (std::streamsize)buf.size());
							if (source.gcount() <= 0) {
								std::cout << "Read error.\n";
							}
							boost::asio::write(socket,
								boost::asio::buffer(buf.c_array(), source.gcount()),
								boost::asio::transfer_all(), error);

							//Print out Chunk info.
							currSize += source.gcount();
							count++;
							std::cout << "Sending Chunk #" << std::setw(5) << std::left << count << ": ";
							std::cout << "Length: " << std::setw(10) << std::left << source.gcount() << " ";
							std::cout << "Cur Size: " << std::setw(10) << std::left << currSize << std::endl;

							if (error) {
								std::cout << "Error: " << error << std::endl;
							}
						}
						else { break; }
					}
					std::cout << "Task complete. " << fileName << " sent. Connection Terminated.\n" << std::endl;		//Success
					i = 4;			//exits out of for-loop
					break;
				}
				else if (input == "2") {
					//partial fail, some code reuse here
					for (int j = 0; j < 3; j++) {
						while (true) {
							if (source.eof() == false) {
								source.read(buf.c_array(), (std::streamsize)buf.size());
								if (source.gcount() <= 0) {
									std::cout << "Read error.\n";
								}

								boost::asio::write(socket,
									boost::asio::buffer(buf.c_array(), source.gcount()),
									boost::asio::transfer_all(), error);

								//Print out Chunk info.
								currSize += source.gcount();
								count++;
								std::cout << "Sending Chunk #" << std::setw(5) << std::left << count << ": ";
								std::cout << "Length: " << std::setw(10) << std::left << source.gcount() << " ";
								std::cout << "Cur Size: " << std::setw(10) << std::left << currSize << std::endl;

								if (error) {
									std::cout << "Error: " << error << std::endl;
								}
							}
							else { break; }
						}
					}

					std::cout << "Task complete. " << fileName << " sent. Connection Terminated.\n" << std::endl;
					//i = 4;			//exits out of for-loop
					//break;
				}
				else if (input == "3") {
					//total fail, some code reuse here
					for (int j = 0; j < 3; j++) {
					
						//read checksum string
						//compare

					}

					std::cout << "Task failed. " << fileName << " not sent. Connection Terminated.\n" << std::endl;
					//i = 4;			//exits out of for-loop
					//break;
				}
				else {
					std::cout << "Invalid input.\n" << std::endl;
				}
			}*/
		}
	}
}

//ugly blocks
void bigCodeBlock1(std::ostream &request_stream, boost::asio::streambuf &request_buf, tcp::socket &socket, std::ifstream &source, boost::system::error_code error, std::string fileName, int i) {
	boost::array<char, chunk_size> buf;
	std::string input = "";
	int currSize = 0;
	int count = 0;
	std::cout << "(1) Successful transfer" << std::endl;
	std::cout << "(2) Partial failure" << std::endl;
	std::cout << "(3) Total failure" << std::endl;
	std::cout << "Please select: ";
	std::getline(std::cin, input);
	std::cout << std::endl;
	
	//success
	if (input == "1") {
		while (true) {
			if (source.eof() == false) {
				source.read(buf.c_array(), (std::streamsize)buf.size());
				if (source.gcount() <= 0) {
					std::cout << "Read error.\n";
				}

				/*boost::asio::write(socket,
					boost::asio::buffer(buf.c_array(), source.gcount()),
					boost::asio::transfer_all(), error);*/

				boost::asio::write(socket,
					boost::asio::buffer(buf.c_array(), source.gcount()),
					boost::asio::transfer_exactly(source.gcount()), error);

				//Send the hash
				boost::array<char, 20> checksum = checkSumHash(buf);
				std::string s(checksum.c_array());
				std::ostream hash_stream(&request_buf);
				hash_stream << s << "\n";
				boost::asio::write(socket, request_buf);

				//Print out Chunk info.
				currSize += source.gcount();
				count++;
				std::cout << "Sending Chunk #" << std::setw(5) << std::left << count << ": ";
				std::cout << "Length: " << std::setw(10) << std::left << source.gcount() << " ";
				std::cout << "Cur Size: " << std::dec << std::setw(10) << std::left << currSize << " ";
				printOutHash(checksum);
				std::cout << std::endl;

				if (error) {
					std::cout << "Error: " << error << std::endl;
				}
			}
			else { break; }
		}
		std::cout << "Task complete. " << fileName << " sent. Connection Terminated.\n" << std::endl;		//Success
		//break;
	}

	//part fail but transfer complete
	else if (input == "2") {
		//partial fail, some code reuse here
		/*for (int j = 0; j < 3; j++) {
			if () {
				std::cout << "Transfer failed! Retrying..." << std::endl;
			}
			else {
				j = 4;
			}
		}*/
		while (true) {
			if (source.eof() == false) {
				source.read(buf.c_array(), (std::streamsize)buf.size());
				if (source.gcount() <= 0) {
					std::cout << "Read error.\n";
				}

				/*boost::asio::write(socket,
				boost::asio::buffer(buf.c_array(), source.gcount()),
				boost::asio::transfer_all(), error);*/

				boost::asio::write(socket,
					boost::asio::buffer(buf.c_array(), source.gcount()),
					boost::asio::transfer_exactly(source.gcount()), error);

				//Send the hash
				boost::array<char, 20> checksum = checkSumHash(buf);
				std::string s(checksum.c_array());
				std::ostream hash_stream(&request_buf);
				hash_stream << s << "\n";
				boost::asio::write(socket, request_buf);

				//Print out Chunk info.
				currSize += source.gcount();
				count++;
				std::cout << "Sending Chunk #" << std::setw(5) << std::left << count << ": ";
				std::cout << "Length: " << std::setw(10) << std::left << source.gcount() << " ";
				std::cout << "Cur Size: " << std::dec << std::setw(10) << std::left << currSize << " ";
				printOutHash(checksum);
				std::cout << std::endl;

				if (error) {
					std::cout << "Error: " << error << std::endl;
				}
			}
			else { break; }
		}
		std::cout << "Task complete. " << fileName << " sent. Connection Terminated.\n" << std::endl;
		//break;
	}
	
	//total fail, abort transfer
	else if (input == "3") {
		//total fail, some code reuse here
		/*for (int j = 0; j < 3; j++) {
			//read checksum string
			//compare	
			if () {
				std::cout << "Transfer failed! Retrying..." << std::endl;
			}
			else {
				j = 4;
			}
		}*/
		std::cout << "Task failed. " << fileName << " not sent. Connection Terminated.\n" << std::endl;
	}
	else {
		std::cout << "Invalid input.\n" << std::endl;
	}
}

//void bigCodeBlock2(std::ostream &request_stream, boost::asio::streambuf &request_buf, tcp::socket &socket, std::ifstream &source, boost::system::error_code error, std::string fileName, int i) {}


// ------------------------------Login-----------------------------------

struct Client
{
	boost::asio::io_service& io_service;
	boost::asio::ip::tcp::socket socket;

	Client(boost::asio::io_service& svc, std::string const& host, std::string const& port)
		: io_service(svc), socket(io_service)
	{
		boost::asio::ip::tcp::resolver resolver(io_service);
		boost::asio::ip::tcp::resolver::iterator endpoint = resolver.resolve(boost::asio::ip::tcp::resolver::query(host, port));
		boost::asio::connect(this->socket, endpoint);
	};

	void send(std::string const& message) {
		socket.send(boost::asio::buffer(message));
	}
};

std::vector<std::string> clientLogIn(){
	//new user?
	std::string response;
	bool invalidResponse = true;
	bool isNewUser = true;
	while (invalidResponse){
		std::cout << "New user? (y/n) " << std::endl;
		std::getline(std::cin, response);
		std::transform(response.begin(), response.end(), response.begin(), ::tolower);
		if (response.compare("y") == 0){
			invalidResponse = false;
			isNewUser = true;
		}
		else if (response.compare("n") == 0){
			invalidResponse = false;
			isNewUser = false;
		}
		else{
			std::cout << "Invalid response....\nPlease try again" << std::endl;
		}
	}


	// New user registration
	//name validation check
	std::string username_input;
	bool invalidName = true;
	while (invalidName){
		if (isNewUser){
			std::cout << "Username can ONLY contain letters and numbers, and is NOT case sensitive." << std::endl;
			std::cout << "\nPlease enter desired username: " << std::endl;
		}
		else{
			std::cout << "Username: " << std::endl;
		}
		std::getline(std::cin, username_input);
		std::transform(username_input.begin(), username_input.end(), username_input.begin(), ::tolower);
		std::regex r("[a-z0-9]+");
		bool match = std::regex_match(username_input, r);
		if (username_input.length() == 0 || !match){
			std::cout << "Username not valid...\nPlease enter a valid username" << std::endl;
			invalidName = true;
		}
		else{ //valid name
			invalidName = false;
		}
	}
	std::string user_password;
	bool invalidPassword = true;
	while (invalidPassword){
		if (isNewUser){
			std::cout << "Password specifications:\n*At least 3 characters long\n*At least 1 uppercase\n*At least 1 number\n*Alphanumeric only" << std::endl;
			std::cout << "Please enter desired password: " << std::endl;
		}
		else{
			std::cout << "Password: " << std::endl;
		}
		std::getline(std::cin, user_password);
		std::regex r("^(?![^a-zA-Z]*$|[^a-z0-9]*$|[^a-z<+$*]*$|[^A-Z0-9]*$|[^A-Z<+$*]*$|[^0-9<+$*]*$|.*[|;{}]).*$");
		bool match = std::regex_match(user_password, r);
		if (user_password.length() == 0 || !match){
			std::cout << "password not valid...\nPlease enter a valid password" << std::endl;
			invalidPassword = true;
		}
		else{// Valid password
			invalidPassword = false;
		}
	}
	std::vector<std::string> v;
	v.push_back(username_input); v.push_back(user_password);
	return v;
}

void client_thread() {
	boost::asio::io_service svc;


	//connect client
	Client client(svc, "127.0.0.1", port);
	//if connection successful ask for login
	std::vector<std::string> v = clientLogIn();
	//send login to server
	std::string sendToServer = v[0] + ":" + v[1];
	client.send(sendToServer);
}

bool checkUser(std::string user, std::string file){
	std::ifstream file_input(file, std::ios::in | std::ios::binary);
	if (!file_input)
		throw std::runtime_error("File not found: " + file);
	std::string line;
	while (std::getline(file_input, line)){
		//if (line.find(user) != std::string::npos){
		if (line.substr(0, line.find(":")).compare(user) == 0){
			return true;
		}
	}

	return false;

}

void server_thread() {
	try
	{
		boost::asio::io_service io_service;
		boost::asio::ip::tcp::acceptor acceptor(io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), std::stoi(port)));

		{
			boost::asio::ip::tcp::socket socket(io_service);
			acceptor.accept(socket);

			boost::asio::streambuf sb;
			boost::system::error_code ec;
			//Connect to port
			while (boost::asio::read(socket, sb, ec)) {
				//const buffer just in case 
				boost::asio::streambuf::const_buffers_type bufs = sb.data();
				//grab information from buffer save into string
				std::string userInfo((std::istreambuf_iterator<char>(&sb)), std::istreambuf_iterator<char>());
				std::string user_name = userInfo.substr(0, userInfo.find(":"));
				std::string user_password = userInfo.substr(userInfo.find(":") + 1);
				std::cout << "user: " << user_name << " user_password: " << user_password << std::endl;
				//file directory vars needed
				std::string filename = "userinfo.txt";
				boost::filesystem::path dir(".");
				boost::filesystem::path file_name(filename);

				//check if file exists else create file
				try{
					boost::filesystem::file_status s = status(file_name);
					if (!boost::filesystem::is_regular_file(s)){
						std::cout << "userinfo.txt created\n";
						boost::filesystem::ofstream ofs(filename);
						ofs << "[start of file <userinfo.txt>]";
					}
					else{
						std::cout << "file exists...\n";
						//file already exists exit
					}
				}
				catch (boost::filesystem::filesystem_error &e){
					std::cerr << e.what() << std::endl;
				}

				//write user info to file
				//check if user is on file
				std::string write_in = "\n" + userInfo;
				if (checkUser(/*userInfo*/user_name, filename)){
					std::cout << "user is here\n";
					//check password
					if (checkUser(user_password, filename))
						std::cout << "authenticated\n";
					else{
						//incorrect password try
						//send message back to client
						std::cout << "incorrect password...\ntry again\n";
						//boost::asio::write(socket, boost::asio::buffer("w_pass"));
						boost::array<char, 256> buf;			//Can be used for all receives
						boost::asio::streambuf request_buf;		//Same deal here
						size_t len = socket.read_some(boost::asio::buffer(buf), ec);
						std::cout.write(buf.data(), len);
					}


				}
				else{
					boost::filesystem::path file(filename);
					boost::filesystem::ofstream ofs;
					ofs.open(file, std::ios::app);
					ofs << write_in;
					std::cout << "Welcome new user\n";
				}

				std::cout << "print: " + userInfo << std::endl;
				if (ec) {
					std::cout << "status: " << ec.message() << "\n";
					break;
				}
			}
		}
	}
	catch (std::exception& e){
		std::cerr << "Exception: " << e.what() << std::endl;
	}
}

// ------------------------------Login-----------------------------------

int _tmain(int argc, _TCHAR* argv[])
{
	std::cout << "\nWelcome to the File Transfer Program.\n" << std::endl;

	bool run = true;
	while (run) {
		std::cout << "1) Start Server\n2) Start Client\n3) Exit Program." << std::endl;
		std::cout << "Please select: ";

		std::string input = "";
		std::getline(std::cin, input);
		std::cout << std::endl;
		if (input == "1") {
			startServer();
		}
		else if (input == "2") {
			std::cout << "Enter IP address of server: ";
			std::getline(std::cin, input);
			startClient(input);
		}
		else if (input == "3") {
			std::cout << "Program exited. Goodbye.";
			run = false;
		}
		else {
			std::cout << "Invalid input.\n" << std::endl;
		}
	}
	return 0;
}