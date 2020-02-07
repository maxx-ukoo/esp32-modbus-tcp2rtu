import React, { Component } from "react";
import FileUploadProgress  from 'react-fileupload-progress';
import axios from 'axios';

class OtaUpdate extends Component {
  constructor(props) {
    super(props);
    this.state = {
      files: [],
      firmware: '',
      uploadFile:''
    }
    this.rebootOnSuccessUpload = this.rebootOnSuccessUpload.bind(this);
  }
  
  componentDidMount() {
    axios.get('/ota/list')
    .then(res => {
      console.log(res.data);
      this.setState({
        files: res.data
      });
    })
  }

  formFirmwareGetter(){
    return new FormData(document.getElementById('uploadFirmwareForm'));
  }

  formFileGetter(){
    return new FormData(document.getElementById('uploadFileForm'));
  }
  

  setUploadFileFile(event) {
    if (event.target.files[0]) {
      this.setState({
        uploadFile: event.target.files[0].name
      });
    }
  }

  setFirmwareFile(event) {
    if (event.target.files[0]) {
      this.setState({
        firmware: event.target.files[0].name
      });
    }
  }

  firmwareFormRenderer(onSubmit){
    let title = 'Choose firmware file';
    if (this.state.firmware) {
      title = this.state.firmware
    }
    return (
      <form id='uploadFirmwareForm'>
        <div className="custom-file">
            <input type="file" name="file" className="custom-file-input" id="firmwareFile" onChange={this.setFirmwareFile.bind(this)}/>
    <label className="custom-file-label" htmlFor="firmwareFile">{title}</label>
        </div>
        <button type="button" className="btn btn-primary" onClick={onSubmit}>Upload</button>
      </form>
    );
  }

  uploadFileFormRenderer(onSubmit){
    let title = 'Choose file';
    if (this.state.uploadFile) {
      title = this.state.uploadFile
    }
    return (
      <form id='uploadFileForm'>
        <div className="custom-file">
            <input type="file" name="file" className="custom-file-input" id="uploadFile" onChange={this.setUploadFileFile.bind(this)}/>
    <label className="custom-file-label" htmlFor="uploadFile">{title}</label>
        </div>
        <button type="button" className="btn btn-primary" onClick={onSubmit}>Upload</button>
      </form>
    );
  }

  customProgressRenderer(progress, hasError, cancelHandler) {
    if (hasError || progress > -1 ) {
      let barStyle = {
        width: progress + '%'
      };

      let message = (<span>{barStyle.width}</span>);
      let error;
      if (hasError) {
        error = (
            <div className="alert alert-danger alert-dismissible fade show">
              <strong>Failed to upload ...!</strong>
            </div>
          );
      }
      if (progress === 100){
        message = (<span >Done</span>);
      }

      return (
        <div>
            <div className="progress">
                <div className="progress-bar" style={barStyle}>{message}</div>
            </div>
            <button className="btn btn-info" onClick={cancelHandler}>
                <span>&times;</span>
            </button>
            {error}
        </div>
      );
    } else {
      return;
    }
  }

  rebootOnSuccessUpload(e, request) {
    if (request.response == "OK") {
      axios.post('/api/reboot')
      .then(res => {
          console.log("OK");
      })
    }
  }

  render() {
    console.log('Files: ' + this.state.files);
    console.log(this.state.files);
    let rows = this.state.files.map((item, index) => {
      return (
          <tr key={item.name}>
            <th scope="col">{item.name}</th>
            <th scope="col">{item.size}</th>
          </tr>
      )
    });
    return (
        <div className="container">
            <h3>Firmware</h3>
            <FileUploadProgress key='ex1' url='/ota/upload/firmware' method='post'
            //onProgress={(e, request, progress) => {console.log('progress', e, request, progress);}}
            onLoad={ (e, request) => this.rebootOnSuccessUpload(e, request)}
            //onError={ (e, request) => {console.log('error', e, request);}}
            //onAbort={ (e, request) => {console.log('abort', e, request);}}
            formGetter={this.formFirmwareGetter.bind(this)}
            formRenderer={this.firmwareFormRenderer.bind(this)}
            progressRenderer={this.customProgressRenderer.bind(this)}
            />
            <div className="border-top my-3"></div>
            <table className="table">
              <thead className="thead-dark">
                <tr>
                  <th scope="col">Name</th>
                  <th scope="col">Size</th>
                </tr>
              </thead>
              <tbody>
                {rows}
              </tbody>
          </table>
          
          <FileUploadProgress key='ex1' url='/ota/upload/file' method='post'
            //onProgress={(e, request, progress) => {console.log('progress', e, request, progress);}}
            onLoad={ (e, request) => this.rebootOnSuccessUpload(e, request)}
            //onError={ (e, request) => {console.log('error', e, request);}}
            //onAbort={ (e, request) => {console.log('abort', e, request);}}
            formGetter={this.formFileGetter.bind(this)}
            formRenderer={this.uploadFileFormRenderer.bind(this)}
            progressRenderer={this.customProgressRenderer.bind(this)}
            />

        </div>
    )
  }
};

export default OtaUpdate;